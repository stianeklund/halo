void scenario_initialize(void)
{
  *(void **)0x5064d0 = game_state_malloc("scenario globals", 0, 0x100);
}

/* Initialize scenario globals for a new map: clear game globals data,
 * copy default gravity/speed from constant table, reset a flag. */
void scenario_initialize_for_new_map(void)
{
  char *globals;

  ((void (*)(void))0x190500)();
  globals = *(char **)0x5064d0;
  csmemset(globals + 4, 0, 0xb0);
  qmemcpy(globals + 0xb8, (void *)0x2c1220, 0x48);
  *(uint8_t *)(globals + 0xb4) = 0;
}

void scenario_dispose_from_old_map(void)
{
  *(char *)0x5057c0 = 0;
}

/* scenario_frame_update is a direct trampoline to the wind update at
 * 0x18ffe0. The original binary has a single JMP instruction here. */
void scenario_frame_update(float delta_time)
{
  ((void (*)(float))0x18ffe0)(delta_time);
}

void scenario_unload(void)
{
  assert_halt(!((bool (*)())0x1c5940)());
  ((void (*)())0x1b9890)();
  *(int *)0x326a08 = NONE;
  *(int16_t *)0x326a0c = NONE;
  *(int16_t *)(*(char **)0x5064d0) = NONE;
  *(int *)0x5064e4 = 0;
  *(int *)0x5064e0 = 0;
  *(int *)0x5064dc = 0;
  *(int *)0x5064d8 = 0;
  *(int *)0x5064d4 = 0;
}

scenario_t *global_scenario_get(void)
{
  assert_halt(*(void **)0x5064e4);
  return *(scenario_t **)0x5064e4;
}

void *scenario_get(void)
{
  assert_halt(global_structure_bsp);
  return global_structure_bsp;
}

void *global_collision_bsp_get(void)
{
  assert_halt(global_collision_bsp);
  return global_collision_bsp;
}

/* Return the game globals tag data pointer. Asserts if not loaded. */
void *game_globals_get(void)
{
  if (!*(void **)0x5064d4) {
    display_assert("global_game_globals",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xdd, 1);
    system_exit(-1);
  }
  return *(void **)0x5064d4;
}

/* Reset a scenario location by setting its cluster_index (offset +6) to
 * NONE (-1), marking it as unresolved/invalid. */
void scenario_location_reset(int *location)
{
  *(int16_t *)((char *)location + 6) = NONE;
}

/* FUN_0018e500 (0x18e500) — look up a material type entry from game globals.
 *
 * Given a material_type index (int16_t), validate it and return a pointer to
 * the corresponding element from the game_globals material_types tag block at
 * offset 0x194. Each element is 0x374 bytes.
 *
 * If game_globals is not loaded, asserts. If material_type is out of range
 * (not NONE and not in [0, NUMBER_OF_MATERIAL_TYPES-1] where
 * NUMBER_OF_MATERIAL_TYPES==33), asserts. If material_type is NONE (-1) or the
 * index is >= the block count, returns a pointer to a static fallback buffer at
 * 0x4d8700, initialising it with a -1 sentinel on the first call.
 *
 * Confirmed: assert "global_game_globals" at line 0xdd (221).
 * Confirmed: assert string "material_type==NONE || ..." at line 0x11e (286).
 * Confirmed: block at game_globals+0x194, element size 0x374.
 * Confirmed: fallback initialises dword at 0x4d8a70 to -1, byte 0x4d8a74 to 1.
 * Confirmed: returns &DAT_004d8700 when index is NONE or out of range.
 */
void *FUN_0018e500(int16_t material_type)
{
  char *game_globals;
  int index;

  if (!*(void **)0x5064d4) {
    display_assert("global_game_globals",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xdd, 1);
    system_exit(-1);
  }
  game_globals = *(char **)0x5064d4;

  if (material_type != (int16_t)NONE &&
      (material_type < 0 || material_type >= 33)) {
    display_assert("material_type==NONE || (material_type>=0 && "
                   "material_type<NUMBER_OF_MATERIAL_TYPES)",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0x11e, 1);
    system_exit(-1);
  }

  if (material_type >= 0) {
    index = (int)material_type;
    if (index < *(int *)(game_globals + 0x194)) {
      return tag_block_get_element(game_globals + 0x194, index, 0x374);
    }
  }

  if (!*(uint8_t *)0x4d8a74) {
    *(int *)0x4d8a70 = NONE;
    *(uint8_t *)0x4d8a74 = 1;
  }
  return (void *)0x4d8700;
}

/* FUN_0018e720 (0x18e720) — bsp3d_find_leaf_point
 *
 * Looks up the BSP3D leaf containing a given 3D point by traversing the
 * bsp3d tree from the root (node 0). Asserts that the global bsp3d tree
 * pointer is non-null before calling.
 *
 * Confirmed: assert "global_bsp3d" at 0xd5 (213).
 * Confirmed: CALL 0x146db0 (bsp3d_find_leaf) with args (bsp3d, 0, point).
 * Confirmed: single cdecl param (point pointer) at [EBP+8].
 * Confirmed: return value is bsp3d_find_leaf result in EAX.
 */
int FUN_0018e720(int point)
{
  if (*(void **)0x5064d8 == NULL) {
    display_assert("global_bsp3d", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xd5, 1);
    system_exit(-1);
  }
  return (int)bsp3d_find_leaf(*(void **)0x5064d8, 0, (void *)point);
}

/* 0x18e770 — Look up the sky tag ref (tag index) for a given sky list index.
 * Returns the 4-byte tag ref from the scenario sky block, or -1 if invalid. */
int FUN_0018e770(int16_t sky_index)
{
  int result;
  char *element;

  if (*(int *)0x5064e4 == 0) {
    display_assert("global_scenario", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xb7, 1);
    system_exit(-1);
  }
  result = -1;
  if ((int)sky_index >= 0) {
    if ((int)sky_index < *(int *)(*(int *)0x5064e4 + 0x30)) {
      element = (char *)tag_block_get_element((int *)(*(int *)0x5064e4 + 0x30),
                                              sky_index, 0x10);
      result = *(int *)(element + 0xc);
    }
  }
  return result;
}

/* 0x18e7d0 — Get sky tag pointer for a given sky list index.
 * Returns tag_get('sky ', tag_ref) or NULL if index is invalid. */
void *FUN_0018e7d0(int param_1)
{
  int tag_ref;

  tag_ref = FUN_0018e770((int16_t)param_1);
  if (tag_ref != -1)
    return tag_get(0x736b7920, tag_ref); /* 'sky ' */
  return NULL;
}

bool scenario_location_potentially_visible_local(void *location)
{
  int16_t cluster_index;
  void *pvs;

  if (*(int16_t *)((char *)location + 4) >= 0) {
    if (!global_structure_bsp) {
      display_assert("global_structure_bsp",
                     "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
      system_exit(-1);
    }
    if ((int)*(int16_t *)((char *)location + 4) <
        *(int *)((char *)global_structure_bsp + 0x134))
      goto valid;
  }
  display_assert(
    "location->cluster_index>=0 && "
    "location->cluster_index<global_structure_bsp_get()->clusters.count",
    "c:\\halo\\SOURCE\\scenario\\scenario.c", 0x1e7, 1);
  system_exit(-1);

valid:
  cluster_index = *(int16_t *)((char *)location + 4);
  pvs = players_get_combined_pvs_local();
  return (*(uint32_t *)((char *)pvs + ((int)cluster_index >> 5) * 4) &
          (1u << (cluster_index & 0x1f))) != 0;
}

/*
 * scenario_get_structure_reference_index_from_tag_index — resolve a BSP3D node
 * index to its fog tag index.
 *
 * Given a bsp3d_node_index, looks up the node element (bsp+0x184, size 0x28)
 * and reads the fog palette index at offset +0x24 (int16_t). If valid, looks
 * up the fog palette entry (bsp+0x190, size 0x88) and returns the fog tag
 * index at offset +0x2c. Returns -1 if any step fails.
 *
 * Confirmed: asserts "global_structure_bsp" at line 0xc5.
 * Confirmed: tag_block_get_element(bsp+0x184, node_index, 0x28) for BSP3D node.
 * Confirmed: tag_block_get_element(bsp+0x190, palette_index, 0x88) for fog
 * palette. Confirmed: returns *(int*)(palette_entry + 0x2c) or -1.
 */
int scenario_get_structure_reference_index_from_tag_index(
  int16_t bsp3d_node_index)
{
  char *bsp;
  char *node_element;
  int16_t fog_palette_index;
  char *palette_entry;

  if (!global_structure_bsp) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  bsp = (char *)global_structure_bsp;

  if (bsp3d_node_index != NONE) {
    node_element =
      (char *)tag_block_get_element(bsp + 0x184, (int)bsp3d_node_index, 0x28);
    fog_palette_index = *(int16_t *)(node_element + 0x24);
    if (fog_palette_index != NONE) {
      palette_entry = (char *)tag_block_get_element(
        bsp + 0x190, (int)fog_palette_index, 0x88);
      if (*(int *)(palette_entry + 0x2c) != -1) {
        return *(int *)(palette_entry + 0x2c);
      }
    }
  }

  return -1;
}

/* Switch the active structure BSP. Calls dispose callbacks on the old BSP,
 * loads the new one from the scenario tag, and calls initialize callbacks.
 * Returns false if the BSP index is invalid or the BSP fails to load. */
bool scenario_switch_structure_bsp(__int16 bsp_index)
{
  char *scenario_tag;
  char *bsp_ref;
  bool had_old_bsp;
  bool result = 0;

  if (bsp_index == *(int16_t *)0x326a0c)
    return result;
  if (bsp_index < 0)
    return result;

  scenario_tag = *(char **)0x5064e4;
  if ((int)bsp_index >= *(int *)(scenario_tag + 0x5a4))
    return result;

  bsp_ref = (char *)((int (*)(void *, int, int))0x19b210)(scenario_tag + 0x5a4,
                                                          (int)bsp_index, 0x20);

  assert_halt(*(char **)0x5064e4);

  /* stop rendering during BSP switch */
  ((void (*)(void))0x101c90)();
  ((void (*)(int))0x14d070)(0);

  had_old_bsp = 0;
  if (*(int16_t *)0x326a0c != -1) {
    /* call 10 dispose-from-old-map callbacks */
    int *dispose_table = (int *)0x326a44;
    int i;
    for (i = 0; i < 10; i++)
      ((void (*)(void))dispose_table[i])();

    /* unload old BSP tag */
    {
      char *old_bsp = (char *)((int (*)(void *, int, int))0x19b210)(
        *(char **)0x5064e4 + 0x5a4, (int)*(int16_t *)0x326a0c, 0x20);
      ((void (*)(void *))0x1ba0c0)(old_bsp);
    }

    *(int16_t *)(*(char **)0x5064d0) = -1;
    *(int16_t *)0x326a0c = -1;
    had_old_bsp = 1;
  }

  /* load new BSP */
  if (((bool (*)(void *))0x1b9fa0)(bsp_ref)) {
    char *bsp_tag = (char *)tag_get(0x73627370, *(int *)(bsp_ref + 0x1c));
    *(char **)0x5064e0 = bsp_tag;

    *(char **)0x5064dc =
      (char *)((int (*)(void *, int, int))0x19b210)(bsp_tag + 0xb0, 0, 0x60);
    *(char **)0x5064d8 =
      (char *)((int (*)(void *, int, int))0x19b210)(bsp_tag + 0xb0, 0, 0x60);

    *(int16_t *)(*(char **)0x5064d0) = bsp_index;
    *(int16_t *)0x326a0c = bsp_index;

    if (had_old_bsp) {
      /* call 13 initialize-for-new-map callbacks */
      int *init_table = (int *)0x326a10;
      int i;
      for (i = 0; i < 13; i++)
        ((void (*)(void))init_table[i])();
    }
    result = 1;
  } else {
    /* BSP load failed */
    error(0, "%s", *(char **)(bsp_ref + 0x14));
  }

  /* resume rendering */
  ((void (*)(int))0x14d070)(1);
  ((void (*)(void))0x101ca0)();

  return result;
}

/* Return the current structure BSP index.
 * Confirmed: MOV AX,[0x326a0c]; RET — 2-instruction getter. */
short global_structure_bsp_index_get(void)
{
  return *(int16_t *)0x326a0c;
}

/* Load a scenario from the map cache. Opens the cache file, loads the
 * scenario and game globals tags, and switches to BSP 0. */
bool scenario_load(const char *map_name)
{
  int tag_index;
  int matg_index;
  char *scenario_tag;
  bool result = 0;

  ((void (*)(void *, const char *))0x8e770)((void *)0x326a6c, "cache\\");
  tag_index = ((int (*)(const char *))0x1b9e70)(map_name);
  *(int *)0x326a08 = tag_index;

  if (tag_index == -1) {
    /* map not found — print error with map path line by line */
    char *path = (char *)0x25386f;
    char *nl;
    error(1, "couldn't open map file");
    do {
      nl = ((char *(*)(const char *, int))0x1d95d0)(path, '\n');
      if (nl)
        *nl = 0;
      error(1, "%s", path);
      if (!nl)
        break;
      *nl = '\n';
      path = nl + 1;
    } while (path);
    return result;
  }

  scenario_tag = (char *)tag_get(0x73636e72, tag_index);
  *(char **)0x5064e4 = scenario_tag;

  if (*(int *)(scenario_tag + 0x5a4) < 1) {
    error(1, "scenario has no structure bsps");
    return result;
  }

  /* load game globals tag ("matg") */
  matg_index = tag_loaded(0x6d617467, "globals\\globals");
  *(char **)0x5064d4 = (char *)tag_get(0x6d617467, matg_index);

  if (scenario_switch_structure_bsp(0))
    return 1;

  return result;
}

/*
 * scenario_location_from_point — resolve a 3D position to a BSP location.
 *
 * Looks up the BSP leaf containing the point via the bsp3d tree, then
 * reads the cluster index from the leaf entry in the structure BSP tag.
 * Writes {leaf_index, cluster_index} into the 8-byte location_out struct.
 *
 * Confirmed: asserts "global_bsp3d" at line 0xd5, "global_structure_bsp"
 *            at line 0xc5.
 * Confirmed: CALL 0x146db0 with 3 args (bsp3d, 0, point) — cdecl.
 * Confirmed: leaf result masked with 0x7fffffff before indexing.
 * Confirmed: tag_block_get_element(bsp+0xe0, leaf & 0x7fffffff, 0x10).
 * Confirmed: cluster_index read as int16 at element+8.
 */
void scenario_location_from_point(void *location_out, void *point)
{
  uint32_t *loc = (uint32_t *)location_out;
  char *element;
  uint32_t leaf;

  if (*(void **)0x5064d8 == 0) {
    display_assert("global_bsp3d", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xd5, 1);
    system_exit(-1);
  }

  leaf = bsp3d_find_leaf(*(void **)0x5064d8, 0, point);
  loc[0] = leaf;

  if (leaf == 0xffffffff) {
    *(int16_t *)&loc[1] = -1;
    return;
  }

  if (*(void **)0x5064e0 == 0) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  element = (char *)tag_block_get_element((char *)*(void **)0x5064e0 + 0xe0,
                                          leaf & 0x7fffffff, 0x10);
  *(int16_t *)&loc[1] = *(int16_t *)(element + 8);
}

/*
 * FUN_0018f2d0 — resolve a BSP location to a fog palette index.
 *
 * Given a location (with cluster_index at offset +4) and a 3D position,
 * looks up the cluster's fog reference from the structure BSP clusters
 * tag block (bsp+0x134, element size 0x68). The fog reference short at
 * offset +2 in the cluster element can be:
 *   - NONE (-1): return NONE.
 *   - Non-negative (bit 15 clear): direct fog palette index (& 0x7fff).
 *   - Negative (bit 15 set): index into the fog planes tag block
 *     (bsp+0x178, element size 0x20). The plane entry contains a fog
 *     palette index at offset +0 and a plane normal+d at offset +4..+0x10.
 *     If the position is on the near side of the plane (dot - d + threshold
 *     < 0.0), returns the fog palette index; otherwise returns NONE.
 *
 * The fog threshold is read from the fog tag (0x666f6720 'fog ') at
 * offset +0x74, but only if the fog tag's first byte has bit 0 set.
 *
 * Confirmed: asserts "global_structure_bsp" at line 0xc5.
 * Confirmed: tag_block_get_element(bsp+0x134, cluster_index, 0x68).
 * Confirmed: tag_block_get_element(bsp+0x178, plane_index, 0x20).
 * Confirmed: FCOMP against 0.0f at [0x2533c0]; TEST AH,5 / JP pattern.
 * Confirmed: calls scenario_get_structure_reference_index_from_tag_index and
 * tag_get('fog ', ...).
 */
int16_t FUN_0018f2d0(void *location, void *position)
{
  char *bsp;
  char *cluster;
  int16_t fog_ref;
  char *plane_entry;
  int16_t fog_palette_index;
  int fog_tag_index;
  char *fog_tag;
  float fog_threshold;

  if (*(int16_t *)((char *)location + 4) == NONE)
    return NONE;

  if (!global_structure_bsp) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  bsp = (char *)global_structure_bsp;
  cluster = (char *)tag_block_get_element(
    bsp + 0x134, (int)*(int16_t *)((char *)location + 4), 0x68);

  fog_ref = *(int16_t *)(cluster + 2);
  if (fog_ref == NONE)
    return NONE;

  if (fog_ref >= 0)
    return fog_ref & 0x7fff;

  plane_entry =
    (char *)tag_block_get_element(bsp + 0x178, (int)(fog_ref & 0x7fff), 0x20);

  fog_palette_index = *(int16_t *)plane_entry;

  fog_tag_index =
    scenario_get_structure_reference_index_from_tag_index(fog_palette_index);
  fog_threshold = 0.0f;
  if (fog_tag_index != -1) {
    fog_tag = (char *)tag_get(0x666f6720, fog_tag_index);
    if (*(uint8_t *)fog_tag & 1) {
      fog_threshold = *(float *)(fog_tag + 0x74);
    }
  }

  if (position == NULL ||
      (*(float *)(plane_entry + 4) * *(float *)position +
       *(float *)(plane_entry + 8) * *((float *)position + 1) +
       *(float *)(plane_entry + 0xc) * *((float *)position + 2)) -
          *(float *)(plane_entry + 0x10) + fog_threshold <
        *(float *)0x2533c0) {
    return fog_palette_index;
  }

  return NONE;
}

/*
 * FUN_0018f3e0 -- determine whether a position is under indoor fog.
 *
 * Given a BSP location and a 3D position, finds the BSP3D node via
 * FUN_0018f2d0, then looks up the corresponding fog tag. Returns true
 * (1) if the fog tag's first byte has bit 0 set (indoor fog flag).
 *
 * Optionally writes a sky index to *out_sky_index: either from the
 * BSP3D node element (offset +0x26), or as a fallback from the cluster
 * element (offset +0x08 in the clusters tag_block at bsp+0x134).
 *
 * Confirmed: asserts "global_structure_bsp" at line 0xc5.
 * Confirmed: asserts "location" at line 0x258 (600).
 * Confirmed: asserts "position" at line 0x259 (601).
 * Confirmed: tag_get('fog ', fog_index) to read indoor flag.
 * Confirmed: tag_block_get_element(bsp+0x184, node, 0x28) for BSP3D node.
 * Confirmed: tag_block_get_element(bsp+0x134, cluster, 0x68) fallback.
 */
bool FUN_0018f3e0(void *location, void *position, int16_t *out_sky_index)
{
  char *bsp;
  int16_t node_index;
  char *node_element;
  char *cluster_element;
  int fog_index;
  char *fog_tag;
  bool is_indoor;
  int16_t sky_index;

  if (!global_structure_bsp) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  bsp = (char *)global_structure_bsp;
  node_index = FUN_0018f2d0(location, position);
  sky_index = NONE;
  is_indoor = false;

  assert_halt(location);
  assert_halt(position);

  if (node_index == NONE) {
    /* no BSP3D node found -- fall through to cluster fallback */
  } else {
    node_element =
      (char *)tag_block_get_element(bsp + 0x184, (int)node_index, 0x28);
    fog_index =
      scenario_get_structure_reference_index_from_tag_index(node_index);
    if (fog_index == -1) {
      is_indoor = false;
    } else {
      fog_tag = (char *)tag_get(0x666f6720, fog_index);
      is_indoor = *(uint8_t *)fog_tag & 1;
    }
    sky_index = *(int16_t *)(node_element + 0x26);
    if (sky_index != NONE)
      goto done;
  }

  /* fallback: read sky index from the cluster */
  if (*(int16_t *)((char *)location + 4) != NONE) {
    cluster_element = (char *)tag_block_get_element(
      bsp + 0x134, (int)*(int16_t *)((char *)location + 4), 0x68);
    sky_index = *(int16_t *)(cluster_element + 0x8);
  }

done:
  if (out_sky_index != NULL) {
    *out_sky_index = sky_index;
  }
  return is_indoor;
}

/*
 * FUN_0018f510 -- compute signed fog distance at a BSP location.
 *
 * Given a BSP location (with cluster_index at offset +4) and a 3D position,
 * looks up the cluster's fog reference from the structure BSP clusters tag
 * block (bsp+0x134, element size 0x68). The fog reference short at offset +2
 * in the cluster element can be:
 *   - NONE (-1): return -FLT_MAX sentinel.
 *   - Negative (bit 15 set): index into the fog planes tag block
 *     (bsp+0x178, element size 0x20) with a plane at offset +4 and a
 *     fog palette index at offset +0.
 *   - Non-negative: direct fog palette index (no plane).
 *
 * The fog palette index is resolved to a fog tag via
 * scenario_get_structure_reference_index_from_tag_index. If the fog tag's first
 * byte has bit 0 set (indoor fog), the function:
 *   - With a plane: returns -(dot(plane, position) + fog_tag[0x74]).
 *   - Without a plane: returns FLT_MAX.
 * Otherwise returns -FLT_MAX.
 *
 * Confirmed: asserts "global_structure_bsp" at line 0xc5.
 * Confirmed: tag_get('fog ', tag_index) at CALL 0x1ba140.
 * Confirmed: plane3d_distance_to_point(plane, position) is plane_test_point
 * (dot - d). Confirmed: FCHS negates the sum before return. Confirmed: 0x2548fc
 * = FLT_MAX (0x7f7fffff). Confirmed: default return = -FLT_MAX (0xff7fffff).
 */
float FUN_0018f510(void *location, void *position)
{
  char *bsp;
  char *cluster;
  int16_t fog_ref;
  int16_t fog_palette_index;
  float *plane;
  int tag_index;
  char *fog_tag;
  float d;

  if (*(int16_t *)((char *)location + 4) == NONE)
    return -3.4028235e+38f;

  if (!global_structure_bsp) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  bsp = (char *)global_structure_bsp;
  cluster = (char *)tag_block_get_element(
    bsp + 0x134, (int)*(int16_t *)((char *)location + 4), 0x68);

  fog_ref = *(int16_t *)(cluster + 2);
  if (fog_ref == NONE)
    return -3.4028235e+38f;

  if (fog_ref < 0) {
    char *palette_entry =
      (char *)tag_block_get_element(bsp + 0x178, (int)(fog_ref & 0x7fff), 0x20);
    fog_palette_index = *(int16_t *)palette_entry;
    plane = (float *)(palette_entry + 4);
  } else {
    fog_palette_index = fog_ref & 0x7fff;
    plane = NULL;
  }

  tag_index =
    scenario_get_structure_reference_index_from_tag_index(fog_palette_index);
  if (tag_index == -1)
    return -3.4028235e+38f;

  fog_tag = (char *)tag_get(0x666f6720, tag_index);
  if (!(*(uint8_t *)fog_tag & 1))
    return -3.4028235e+38f;

  if (plane != NULL) {
    d = plane3d_distance_to_point(plane, position);
    return -(d + *(float *)(fog_tag + 0x74));
  }

  return 3.4028235e+38f;
}
