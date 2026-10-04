/* 0x1929a0 -- Project a leaf face's 2D vertex back onto its 3D node plane.
 * The projection axis is the plane normal's largest absolute component
 * (z, then y, else x); the sign byte comes from FUN_00099270. */
void leaf_face_get_vertex3d(int *collision_bsp_ref, int *leaf_face,
                            int16_t vertex_index, float *point)
{
  int *node;
  float *plane;
  float abs_x;
  float abs_y;
  float abs_z;
  int projection;
  uint8_t sign;

  node =
    (int *)tag_block_get_element((void *)*collision_bsp_ref, *leaf_face, 0xc);
  plane = (float *)tag_block_get_element((void *)(*collision_bsp_ref + 0xc),
                                         *node, 0x10);
  abs_x = (float)fabs(plane[0]);
  abs_y = (float)fabs(plane[1]);
  abs_z = (float)fabs(plane[2]);
  if (abs_z >= abs_y && abs_z >= abs_x) {
    projection = 2;
  } else if (abs_y >= abs_x) {
    projection = 1;
  } else {
    projection = 0;
  }
  sign = (uint8_t)FUN_00099270(plane, (short)projection);
  project_point2d(
    (float *)tag_block_get_element((void *)(leaf_face + 1), vertex_index, 8),
    plane, (int16_t)projection, sign, point);
}

/* 0x192a50 -- Debug-draw each face of a leaf-map leaf: a triangle fan
 * filled with the local color {0.2, 1, 0, 0} plus outline lines in the
 * color pointer stored at 0x2ee6c4. Leaf elements are 0x18 bytes, face
 * elements 0x10 bytes with the vertex count at +0x04. */
void render_debug_leaf_faces(int *leaf_map, int leaf_index)
{
  int *leaf;
  int *face;
  float color[4];
  float point0[3];
  float point1[3];
  float point2[3];
  float previous[3];
  int16_t face_index;
  int16_t vertex_index;

  leaf = (int *)tag_block_get_element((void *)(leaf_map + 1),
                                      leaf_index & 0x7fffffff, 0x18);
  color[0] = 0.2f;
  color[1] = 1.0f;
  color[2] = 0.0f;
  color[3] = 0.0f;
  if (*leaf_map == 0) {
    display_assert("map->bsp", "c:\\halo\\SOURCE\\structures\\leaf_map.c",
                   0x3ac, 1);
    system_exit(-1);
  }
  for (face_index = 0; face_index < *leaf; face_index++) {
    face = (int *)tag_block_get_element(leaf, face_index, 0x10);
    leaf_face_get_vertex3d(leaf_map, face, 0, point0);
    leaf_face_get_vertex3d(leaf_map, face, 1, point1);
    previous[0] = point1[0];
    previous[1] = point1[1];
    previous[2] = point1[2];
    FUN_00189270(1, point0, point1, *(void **)0x2ee6c4);
    for (vertex_index = 2; vertex_index < face[1]; vertex_index++) {
      leaf_face_get_vertex3d(leaf_map, face, vertex_index, point2);
      FUN_00188890(1, point0, previous, point2, color);
      FUN_00189270(1, previous, point2, *(void **)0x2ee6c4);
      previous[0] = point2[0];
      previous[1] = point2[1];
      previous[2] = point2[2];
    }
  }
}

/* 0x192da0 -- Recurse down a leaf-map node's children building portals
 * between the given leaf and every other leaf reached. */
void leaf_map_build_portals_from_leaf(int *leaf_map, int ancestor_node_index,
                                      int leaf_index, int node_index,
                                      int16_t levels_up)
{
  int *node;
  int first_traversal_node;
  char found;
  char found_child_index;
  int16_t child_index;
  bool from_node;
  int *leaf;
  int16_t face_index;
  int16_t matching_face_index;
  int child;

  node = (int *)tag_block_get_element((void *)*leaf_map, node_index, 0xc);
  if (ancestor_node_index == NONE) {
    if (levels_up < 0 || levels_up >= *(int16_t *)0x4d8e90) {
      display_assert(
        "levels_up>=0 && levels_up<leaf_map_globals.node_stack_count",
        "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x3b, 1);
      system_exit(-1);
    }
    first_traversal_node =
      *(int *)(0x4d8a8c + ((int)*(int16_t *)0x4d8e90 - (int)levels_up) * 4);
  } else {
    first_traversal_node = NONE;
  }
  found = FUN_00191bd0(*node, (void **)leaf_map, &found_child_index);
  if (ancestor_node_index == NONE &&
      (first_traversal_node & 0x7fffffff) != node_index) {
    display_assert("ancestor_node_index!=NONE || "
                   "index_from_node(first_traversal_node)==node_index",
                   "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x19f, 1);
    system_exit(-1);
  }
  for (child_index = 0; child_index < 2; child_index++) {
    if (ancestor_node_index == NONE && child_index != 0 &&
        first_traversal_node < 0) {
      from_node = 1;
    } else {
      from_node = 0;
    }
    if (ancestor_node_index == NONE) {
      if (child_index == 0 && first_traversal_node >= 0) {
        continue;
      }
      if (from_node) {
        leaf = (int *)tag_block_get_element((void *)(leaf_map + 1),
                                            leaf_index & 0x7fffffff, 0x18);
        matching_face_index = NONE;
        for (face_index = 0; face_index < *leaf; face_index++) {
          if (*(int *)tag_block_get_element(leaf, face_index, 0x10) ==
              node_index) {
            matching_face_index = face_index;
            break;
          }
        }
        if (matching_face_index == NONE) {
          continue;
        }
      }
    } else if (found &&
               (int16_t)(unsigned char)found_child_index == child_index) {
      continue;
    }
    child = node[child_index + 1];
    if (child < 0) {
      if (child != NONE && (child & 0x7fffffff) != leaf_index) {
        leaf_map_build_portal_from_leaves(
          leaf_map, from_node ? node_index : ancestor_node_index, leaf_index,
          child);
      }
    } else {
      leaf_map_build_portals_from_leaf(
        leaf_map, from_node ? node_index : ancestor_node_index, leaf_index,
        child, (int16_t)(levels_up - 1));
    }
  }
}

/* 0x192f80 polygon working buffer: 0x204 bytes (REP MOVSD 0x81 dwords from
 * the template at 0x3271e0); int16 count at +0x00, 64 2D points at +0x04
 * (clip max_count 0x40, csmemcpy size count<<3). */
typedef struct leaf_map_polygon2d {
  int16_t vertex_count;
  uint8_t pad_02[2];
  float vertices[64][2];
} leaf_map_polygon2d;

/* 0x192f80 -- Build the 2D leaf face lying on a node's plane: start from the
 * template polygon, clip it against the intersection line of every other
 * plane on the node stack, then store it as a new face of the leaf.
 * intersect_planes3d returns 1 (line found -> clip), 0 (empty -> discard). */
void leaf_map_build_leaf_face_for_leaf_on_node(int *leaf_map, int leaf_index,
                                               int node_reference)
{
  leaf_map_polygon2d polygon;
  float line[3];
  float *plane;
  int node_index;
  float other_plane[4];
  int *node;
  float *source_plane;
  int other_reference;
  int16_t levels_up;
  int16_t result;
  void *leaf;
  int16_t face_index;
  int *face;

  node_index = node_reference & 0x7fffffff;
  node = (int *)tag_block_get_element((void *)*leaf_map, node_index, 0xc);
  plane =
    (float *)tag_block_get_element((void *)(*leaf_map + 0xc), *node, 0x10);
  polygon = *(leaf_map_polygon2d *)0x3271e0;
  for (levels_up = 0;
       levels_up < *(int16_t *)0x4d8e90 && polygon.vertex_count != 0;
       levels_up++) {
    if (levels_up < 0 || levels_up >= *(int16_t *)0x4d8e90) {
      display_assert(
        "levels_up>=0 && levels_up<leaf_map_globals.node_stack_count",
        "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x3b, 1);
      system_exit(-1);
    }
    other_reference =
      *(int *)(0x4d8a8c + ((int)*(int16_t *)0x4d8e90 - (int)levels_up) * 4);
    if (other_reference != node_reference) {
      node = (int *)tag_block_get_element((void *)*leaf_map,
                                          other_reference & 0x7fffffff, 0xc);
      source_plane =
        (float *)tag_block_get_element((void *)(*leaf_map + 0xc), *node, 0x10);
      other_plane[0] = source_plane[0];
      other_plane[1] = source_plane[1];
      other_plane[2] = source_plane[2];
      other_plane[3] = source_plane[3];
      if (other_reference < 0) {
        other_plane[0] = -other_plane[0];
        other_plane[1] = -other_plane[1];
        other_plane[2] = -other_plane[2];
        other_plane[3] = -other_plane[3];
      }
      result = intersect_planes3d(plane, other_plane, line);
      if (result == 1) {
        polygon.vertex_count = convex_polygon2d_clip_to_plane(
          polygon.vertex_count, &polygon.vertices[0][0], line, 0x40,
          &polygon.vertices[0][0], NULL, NULL, 0.00024414063f);
        if (polygon.vertex_count == -1) {
          display_assert("result.vertex_count!=NONE",
                         "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0xe3, 1);
          system_exit(-1);
        }
      } else if (result == 0) {
        polygon.vertex_count = 0;
      }
    }
  }
  if (polygon.vertex_count != 0) {
    leaf = tag_block_get_element((void *)(leaf_map + 1),
                                 leaf_index & 0x7fffffff, 0x18);
    face_index = tag_block_add_element(leaf);
    if (face_index == -1) {
      if (*(char **)0x4d8e94 == NULL) {
        *(char **)0x4d8e94 = "couldn't allocate leaf face.";
      }
    } else {
      face = (int *)tag_block_get_element(leaf, face_index, 0x10);
      face[0] = node_index;
      if (tag_block_resize((void *)(face + 1), polygon.vertex_count)) {
        csmemcpy((void *)face[2], &polygon.vertices[0][0],
                 polygon.vertex_count << 3);
      } else if (*(char **)0x4d8e94 == NULL) {
        *(char **)0x4d8e94 = "couldn't allocate leaf vertices.";
      }
    }
  }
}

/* 0x1931e0 -- Push each child of a leaf-map node onto the node stack and
 * recurse; negative (non -1) children build portals from the leaf.
 * The fifth callee arg is the node-stack count minus one (dword read of the
 * int16 global, DEC, PUSH). */
void leaf_map_build_portals(int *leaf_map, int node_index)
{
  int *node;
  int child;
  int entry;
  int16_t i;

  node = (int *)tag_block_get_element((void *)*leaf_map, node_index, 0xc);
  for (i = 0; i < 2; i = (int16_t)(i + 1)) {
    entry = node_index;
    if (i == 0) {
      entry |= (int)0x80000000;
    }
    if (*(int16_t *)0x4d8e90 >= 0x100) {
      display_assert(
        "leaf_map_globals.node_stack_count<MAXIMUM_NODE_STACK_COUNT",
        "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x2a, 1);
      system_exit(-1);
    }
    *(int *)(0x4d8a90 + (int)*(int16_t *)0x4d8e90 * 4) = entry;
    ++*(int16_t *)0x4d8e90;
    child = node[i + 1];
    if (child < 0) {
      if (child != -1) {
        leaf_map_build_portals_from_leaf(leaf_map, -1, child & 0x7fffffff, 0,
                                         (int16_t)(*(int16_t *)0x4d8e90 - 1));
      }
    } else {
      leaf_map_build_portals(leaf_map, child);
    }
    if (*(int16_t *)0x4d8e90 <= 0) {
      display_assert("leaf_map_globals.node_stack_count>0",
                     "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x33, 1);
      system_exit(-1);
    }
    --*(int16_t *)0x4d8e90;
  }
}

/* Traverse two child nodes from a BSP leaf (0x193340). */
void FUN_00193340(int *leaf_map, int leaf_index)
{
  int *leaf;
  int node;
  int16_t i;

  leaf = (int *)tag_block_get_element((void *)*leaf_map, leaf_index, 0xc);
  for (i = 0; i < 2; i = (int16_t)(i + 1)) {
    node = leaf_index;
    if (i == 0) {
      node |= (int)0x80000000;
    }
    if (*(int16_t *)0x4d8e90 >= 0x100) {
      display_assert(
        "leaf_map_globals.node_stack_count<MAXIMUM_NODE_STACK_COUNT",
        "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x2a, 1);
      system_exit(-1);
    }
    *(int *)(0x4d8a90 + (int)*(int16_t *)0x4d8e90 * 4) = node;
    ++*(int16_t *)0x4d8e90;
    node = leaf[i + 1];
    if (node < 0) {
      if (node != -1) {
        FUN_001932d0(leaf_map, node);
      }
    } else {
      FUN_00193340(leaf_map, node);
    }
    if (*(int16_t *)0x4d8e90 <= 0) {
      display_assert("leaf_map_globals.node_stack_count>0",
                     "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x33, 1);
      system_exit(-1);
    }
    --*(int16_t *)0x4d8e90;
  }
}

/* 0x193420 -- Bind a leaf map to its BSP, size the leaves block, then
 * walk the BSP from node 0 building leaves and portals. Errors recorded
 * in leaf_map_globals.error (0x4d8e94) are reported via error(1, ...).
 * Returns true when no error was recorded. */
bool leaf_map_initialize_from_bsp(int *leaf_map, int *bsp, int leaf_count)
{
  if (leaf_map == NULL) {
    display_assert("leaf_map", "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x56,
                   1);
    system_exit(-1);
  }
  if (bsp == NULL) {
    display_assert("bsp", "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x57, 1);
    system_exit(-1);
  }
  if (*(int16_t *)0x4d8e90 != 0) {
    display_assert("leaf_map_globals.node_stack_count==0",
                   "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x58, 1);
    system_exit(-1);
  }
  *(char **)0x4d8e94 = NULL;
  if (*(char *)0x449ef1 != 0 && *(char *)0x326bf0 != 0) {
    profile_enter_private((void *)0x326be8);
  }
  *leaf_map = (int)bsp;
  if (tag_block_resize((void *)(leaf_map + 1), leaf_count)) {
    if (*bsp > 0) {
      FUN_00193340(leaf_map, 0);
      leaf_map_build_portals(leaf_map, 0);
    }
  } else if (*(char **)0x4d8e94 == NULL) {
    *(char **)0x4d8e94 = "couldn't allocate leaf_map leaves.";
  }
  if (*(char **)0x4d8e94 != NULL) {
    error(1, *(char **)0x4d8e94);
  }
  if (*(char *)0x449ef1 != 0 && *(char *)0x326bf0 != 0) {
    profile_exit_private((void *)0x326be8);
  }
  return *(char **)0x4d8e94 == NULL;
}

/* Return pointer to a cluster's sound bit-vector data (0x193550).
 * Computes BIT_VECTOR_SIZE_IN_LONGS from clusters.count, then indexes
 * into cluster_data.elements by cluster_index * that stride. */
uint32_t *structure_bsp_get_cluster_sound_data(void *bsp, int16_t cluster_index)
{
  char *b = (char *)bsp;

  if (cluster_index < 0 || (int)cluster_index >= *(int *)(b + 0x134)) {
    display_assert(
      "cluster_index>=0 && cluster_index<structure_bsp->clusters.count",
      "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c", 0x24, 1);
    system_exit(-1);
  }

  if ((cluster_index + 1) * ((*(int *)(b + 0x134) + 31) >> 5) >
      *(int *)(b + 0x140)) {
    display_assert("(cluster_index+1)*BIT_VECTOR_SIZE_IN_LONGS(structure_bsp->"
                   "clusters.count)<=structure_bsp->cluster_data.size",
                   "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c",
                   0x25, 1);
    system_exit(-1);
  }

  return (uint32_t *)(*(int *)(b + 0x14c) + ((*(int *)(b + 0x134) + 31) >> 5) *
                                              (int)cluster_index * 4);
}

/* 0x1935f0 — Locate the lightmap collection and material containing a surface.
 */
void structure_bsp_find_material_for_surface(void *scenario, int surface_index,
                                             int16_t *out_collection_index,
                                             int16_t *out_geometry_index)
{
  char *collection;
  char *material;
  char *last_material;
  tag_block *materials;
  int16_t collection_high;
  int16_t collection_low;
  int16_t material_high;
  int16_t material_low;
  int16_t index;

  collection_low = 0;
  *out_collection_index = 0;
  collection_high = *(int16_t *)((char *)scenario + 0x104) - 1;
  if (collection_high > 0) {
    do {
      index = (int16_t)(((int)collection_high - (int)collection_low) / 2) +
              collection_low;
      *out_collection_index = index;
      collection = (char *)tag_block_get_element((char *)scenario + 0x104,
                                                 (int)index, 0x20);
      materials = (tag_block *)(collection + 0x14);
      material = (char *)tag_block_get_element(materials, 0, 0x100);
      if (surface_index < *(int *)(material + 0x14)) {
        collection_high = *out_collection_index - 1;
        *out_collection_index = collection_high;
      } else {
        material =
          (char *)tag_block_get_element(materials, materials->count - 1, 0x100);
        last_material =
          (char *)tag_block_get_element(materials, materials->count - 1, 0x100);
        if (surface_index <
            *(int *)(material + 0x18) + *(int *)(last_material + 0x14)) {
          break;
        }
        collection_low = *out_collection_index + 1;
        *out_collection_index = collection_low;
      }
    } while (collection_low < collection_high);
  }

  collection = (char *)tag_block_get_element((char *)scenario + 0x104,
                                             (int)*out_collection_index, 0x20);
  materials = (tag_block *)(collection + 0x14);
  material_low = 0;
  *out_geometry_index = 0;
  material_high = materials->count;
  if (material_high > 0) {
    do {
      index =
        (int16_t)(((int)material_high - (int)material_low) / 2) + material_low;
      *out_geometry_index = index;
      material = (char *)tag_block_get_element(materials, (int)index, 0x100);
      if (surface_index < *(int *)(material + 0x14)) {
        material_high = *out_geometry_index - 1;
        *out_geometry_index = material_high;
      } else {
        if (surface_index <
            *(int *)(material + 0x18) + *(int *)(material + 0x14)) {
          break;
        }
        material_low = *out_geometry_index + 1;
        *out_geometry_index = material_low;
      }
    } while (material_low < material_high);
  }

  material =
    (char *)tag_block_get_element(materials, (int)*out_geometry_index, 0x100);
  if (surface_index < *(int *)(material + 0x14)) {
    display_assert("surface_index>=material->first_surface_index",
                   "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c",
                   0x66, 1);
    system_exit(-1);
  }
  if (surface_index >= *(int *)(material + 0x18) + *(int *)(material + 0x14)) {
    display_assert(
      "surface_index<material->first_surface_index+material->surface_count",
      "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c", 0x67, 1);
    system_exit(-1);
  }
}

/* 0x1937a0 — Select the two vertex type codes for a shader.
 * param_1 is never read by the binary. The flag byte at [ebp+0x14]
 * selects the pair (1, 3) when set, otherwise (0, 2). Both outputs are
 * 16-bit stores. */
void vertex_type_from_shader_tag(int param_1, int16_t *out_type_a,
                                 int16_t *out_type_b, bool flag)
{
  if (flag) {
    *out_type_a = 1;
    *out_type_b = 3;
    return;
  }
  *out_type_a = 0;
  *out_type_b = 2;
}

/* Return a pointer to the sound encoding byte for a cluster pair (0x1937d0).
 * Uses upper-triangular matrix indexing (row < column, no diagonal)
 * into sound_cluster_data. */
uint8_t *structure_bsp_get_cluster_encoded_sound_data(void *bsp,
                                                      int16_t from_cluster,
                                                      int16_t to_cluster)
{
  char *b = (char *)bsp;
  int16_t offset;

  offset = (int16_t)((*(int16_t *)(b + 0x134) - 1) * from_cluster -
                     (from_cluster + 1) * from_cluster / 2 + to_cluster - 1);

  if (from_cluster >= to_cluster) {
    display_assert("row_index<column_index",
                   "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c",
                   0x4b2, 1);
    system_exit(-1);
  }

  if (offset < 0 || offset >= *(int *)(b + 0x214)) {
    display_assert("offset>=0 && offset<structure_bsp->sound_cluster_data.size",
                   "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c",
                   0x4b3, 1);
    system_exit(-1);
  }

  return (uint8_t *)(*(int *)(b + 0x220) + offset);
}

/* Look up the sound encoding byte between two clusters (0x193870).
 * Ensures from < to by swapping if necessary, then delegates to
 * structure_bsp_get_cluster_encoded_sound_data for the actual lookup. Returns 0
 * for same-cluster. */
uint8_t structure_bsp_cluster_sound_encoding(void *bsp, int16_t from_cluster,
                                             int16_t to_cluster)
{
  char *b = (char *)bsp;
  int16_t tmp;

  if (from_cluster < 0 || (int)from_cluster >= *(int *)(b + 0x134)) {
    display_assert("from_cluster_index>=0 && from_cluster_index<structure_bsp->"
                   "clusters.count",
                   "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c",
                   0x4bf, 1);
    system_exit(-1);
  }
  if (to_cluster < 0 || (int)to_cluster >= *(int *)(b + 0x134)) {
    display_assert("to_cluster_index>=0 && to_cluster_index<structure_bsp->"
                   "clusters.count",
                   "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c",
                   0x4c0, 1);
    system_exit(-1);
  }

  if (from_cluster != to_cluster) {
    if (from_cluster > to_cluster) {
      tmp = from_cluster;
      from_cluster = to_cluster;
      to_cluster = tmp;
    }

    return *structure_bsp_get_cluster_encoded_sound_data(bsp, from_cluster,
                                                         to_cluster);
  }

  return 0;
}
