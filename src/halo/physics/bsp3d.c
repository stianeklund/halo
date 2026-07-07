/* bsp3d_find_leaf (0x146db0): descend the BSP3D node tree from `root` to the
 * leaf containing `point`.
 *
 * Two tag_blocks live in the bsp3d structure: the nodes block at base+0
 * (stride 0xc = 3 dwords: [0]=plane index, [1]=back child, [2]=front child)
 * and the planes block at base+0xc (stride 0x10 = 4 floats: [0..2]=normal xyz,
 * [3]=plane distance d).
 *
 * At each node we compute the signed distance dot(normal, point) - d. If it is
 * >= 0.0f we take the front child (index 2), otherwise the back child (index 1).
 * The multiply-add is emitted in x,z,y source order to match the original's
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
    plane = (float *)tag_block_get_element((char *)bsp3d + 0xc, (int)node[0], 0x10);
    dist = (plane[0] * p[0] + plane[2] * p[2] + plane[1] * p[1]) - plane[3];
    node_index = node[1 + (0.0f <= dist ? 1 : 0)];
  } while ((int)node_index >= 0);

  if (node_index != 0xffffffff) {
    return node_index & 0x7fffffff;
  }
  return 0xffffffff;
}
