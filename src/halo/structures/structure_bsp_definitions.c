/* Traverse two child nodes from a BSP leaf (0x193340). */
void leaf_map_build_leaf_faces(int *leaf_map, int leaf_index)
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
      display_assert("leaf_map_globals.node_stack_count<MAXIMUM_NODE_STACK_COUNT",
                     "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x2a, 1);
      system_exit(-1);
    }
    *(int *)(0x4d8a90 + (int)*(int16_t *)0x4d8e90 * 4) = node;
    ++*(int16_t *)0x4d8e90;
    node = leaf[i + 1];
    if (node < 0) {
      if (node != -1) {
        leaf_map_build_leaf_faces_for_leaf(leaf_map, node);
      }
    } else {
      leaf_map_build_leaf_faces(leaf_map, node);
    }
    if (*(int16_t *)0x4d8e90 <= 0) {
      display_assert("leaf_map_globals.node_stack_count>0",
                     "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x33, 1);
      system_exit(-1);
    }
    --*(int16_t *)0x4d8e90;
  }
}

/* Return pointer to a cluster's sound bit-vector data (0x193550).
 * Computes BIT_VECTOR_SIZE_IN_LONGS from clusters.count, then indexes
 * into cluster_data.elements by cluster_index * that stride. */
uint32_t *structure_bsp_get_cluster_pvs(void *bsp, int16_t cluster_index)
{
  char *b = (char *)bsp;

  if (cluster_index < 0 || (int)cluster_index >= *(int *)(b + 0x134)) {
    display_assert(
      "cluster_index>=0 && cluster_index<structure_bsp->clusters.count",
      "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c", 0x24, 1);
    system_exit(-1);
  }

  if ((cluster_index + 1) * ((*(int *)(b + 0x134) + 31) >> 5) > *(int *)(b + 0x140)) {
    display_assert("(cluster_index+1)*BIT_VECTOR_SIZE_IN_LONGS(structure_bsp->"
                   "clusters.count)<=structure_bsp->cluster_data.size",
                   "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c",
                   0x25, 1);
    system_exit(-1);
  }

  return (uint32_t *)(*(int *)(b + 0x14c) +
                      ((*(int *)(b + 0x134) + 31) >> 5) * (int)cluster_index * 4);
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
uint8_t structure_bsp_get_cluster_encoded_sound_distance(void *bsp, int16_t from_cluster,
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

/* 0x1935f0 — Locate the lightmap collection and material containing a surface. */
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
        material = (char *)tag_block_get_element(materials,
                                                  materials->count - 1, 0x100);
        last_material = (char *)tag_block_get_element(
          materials, materials->count - 1, 0x100);
        if (surface_index < *(int *)(material + 0x18) +
                            *(int *)(last_material + 0x14)) {
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
      index = (int16_t)(((int)material_high - (int)material_low) / 2) +
              material_low;
      *out_geometry_index = index;
      material = (char *)tag_block_get_element(materials, (int)index, 0x100);
      if (surface_index < *(int *)(material + 0x14)) {
        material_high = *out_geometry_index - 1;
        *out_geometry_index = material_high;
      } else {
        if (surface_index < *(int *)(material + 0x18) +
                            *(int *)(material + 0x14)) {
          break;
        }
        material_low = *out_geometry_index + 1;
        *out_geometry_index = material_low;
      }
    } while (material_low < material_high);
  }

  material = (char *)tag_block_get_element(materials,
                                             (int)*out_geometry_index, 0x100);
  if (surface_index < *(int *)(material + 0x14)) {
    display_assert("surface_index>=material->first_surface_index",
                   "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c",
                   0x66, 1);
    system_exit(-1);
  }
  if (surface_index >= *(int *)(material + 0x18) +
                       *(int *)(material + 0x14)) {
    display_assert("surface_index<material->first_surface_index+material->surface_count",
                   "c:\\halo\\SOURCE\\structures\\structure_bsp_definitions.c",
                   0x67, 1);
    system_exit(-1);
  }
}
