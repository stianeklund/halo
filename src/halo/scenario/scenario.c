/* 0x18aef0 — object bounding-sphere accessor: fetch an object's world-space
 * center (object_data + 0x50/0x54/0x58, three contiguous floats) and the
 * bounding radius from its 'obje' definition tag (+0x104). The object handle
 * is resolved with no type restriction (0xffffffff). Asserts that both output
 * pointers are non-NULL. cdecl: handle [EBP+8], center [EBP+0xc] (-> ESI),
 * radius [EBP+0x10] (-> EBX); plain MOV dword copies reinterpreted as float. */
void FUN_0018aef0(int object_handle, float *center, float *radius)
{
  char *obj;
  char *def_tag;

  obj = (char *)object_get_and_verify_type(object_handle, 0xffffffff);
  if (center == 0) {
    display_assert("center", "..\\objects\\objects.h", 0x227, 1);
    system_exit(-1);
  }
  if (radius == 0) {
    display_assert("radius", "..\\objects\\objects.h", 0x228, 1);
    system_exit(-1);
  }
  center[0] = *(float *)(obj + 0x50);
  center[1] = *(float *)(obj + 0x54);
  center[2] = *(float *)(obj + 0x58);
  def_tag = (char *)tag_get(0x6f626a65 /* 'obje' */, *(int *)obj);
  *radius = *(float *)(def_tag + 0x104);
}

/* 0x18af90 — cached object render-states pool: allocate the game-state
 * data array (0x100 entries x 0x100 bytes) and store it at the global
 * pool pointer; assert on allocation failure. */
void FUN_0018AF90(void)
{
  *(data_t **)0x50652c =
      game_state_data_new("cached object render states", 0x100, 0x100);
  if (*(data_t **)0x50652c == 0) {
    display_assert("cached_object_render_states",
                   "c:\\halo\\SOURCE\\render\\render_objects.c", 0x7d, 1);
    system_exit(-1);
  }
}

/* 0x18afd0 — cached object render-states pool free: delete all entries in the
 * cached_object_render_states data pool. */
void FUN_0018afd0(void)
{
  data_delete_all(*(data_t **)0x50652c);
}

/* 0x18afe0 — cached object render-states pool dispose: if the pool exists
 * and is initialized (+0x24 valid flag set), invalidate its data array. */
void FUN_0018afe0(void)
{
  if (*(data_t **)0x50652c != 0 &&
      *(char *)((char *)*(data_t **)0x50652c + 0x24) != 0) {
    data_make_invalid(*(data_t **)0x50652c);
  }
}

/* 0x18b000 — cached object render-states pool clear: reset the global pool
 * pointer to NULL. */
void FUN_0018B000(void)
{
  *(data_t **)0x50652c = 0;
}

/* 0x18b080 — rebuild the per-frame rendered-object list. Resets the object
 * marker pass, gathers up to 0x100 collideable then noncollideable objects
 * across all clusters into the int[256] table at 0x4d82d4 (count tracked in
 * the int16 counter at 0x4d82d0), ends the marker pass, then errors once if
 * the table filled to MAXIMUM_RENDERED_OBJECTS (0x100). cdecl, void(void). */
void FUN_0018b080(void)
{
  short count;

  object_reset_markers();
  count = (short)FUN_00196c90((int)0x4d82d4, 0x100,
                              (void *)cluster_partition_object_iter_first,
                              (void *)cluster_partition_object_iter_next,
                              (void *)FUN_0018aef0,
                              (void *)object_markers_need_update,
                              (void *)object_mark);
  *(short *)0x4d82d0 = count;
  count = (short)FUN_00196c90(0x4d82d4 + (int)count * 4,
                              0x100 - *(unsigned short *)0x4d82d0,
                              (void *)cluster_get_first_noncollideable_object,
                              (void *)cluster_get_next_noncollideable_object,
                              (void *)FUN_0018aef0,
                              (void *)object_markers_need_update,
                              (void *)object_mark);
  *(short *)0x4d82d0 = (short)(*(short *)0x4d82d0 + count);
  object_marker_end();
  if (*(unsigned short *)0x4d82d0 == 0x100 && *(char *)0x4d86d5 == 0) {
    error(2, "MAXIMUM_RENDERED_OBJECTS exceeded.");
    *(char *)0x4d86d5 = 1;
  }
}

/* 0x18c580 — 3-key record comparator: compares two records by signed int16 at
 * +2, then signed int16 at +4, then unsigned byte at +6; returns the first
 * non-zero difference (qsort/bsearch-style ordering). */
int FUN_0018c580(int param_1, int param_2)
{
  int diff;

  diff = (int)*(short *)(param_1 + 2) - (int)*(short *)(param_2 + 2);
  if (diff == 0) {
    diff = (int)*(short *)(param_1 + 4) - (int)*(short *)(param_2 + 4);
    if (diff == 0)
      diff = (int)*(unsigned char *)(param_1 + 6) -
             (int)*(unsigned char *)(param_2 + 6);
  }
  return diff;
}


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

/* Return the global scenario pointer without asserting (0x18e3b0).
 * Sibling of global_scenario_get; used at call sites that must tolerate
 * a NULL scenario (e.g. early boot, between map switches). Compiles to
 * a 2-instruction MOV EAX, [global]; RET. */
void *FUN_0018e3b0(void)
{
  return *(void **)0x5064e4;
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

/* 0x18e690 — returns a constant float (0.0f at 0x2533c0). */
float FUN_0018e690(void)
{
  return 0.0f;
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

/* 0x18ea50 — find a named entry in a scenario tag's name block (param_1+0x204,
 * element size 0x24) by csstrcmp against `name`; returns the 0-based index or
 * -1 (NONE) if no match. Block count is re-read from *(int*)(param_1+0x204)
 * each iteration. */
int16_t FUN_0018ea50(void *param_1, const char *name)
{
  int16_t i;
  void *element;

  i = 0;
  if (*(int *)((char *)param_1 + 0x204) > 0) {
    do {
      element = tag_block_get_element((char *)param_1 + 0x204, i, 0x24);
      if (csstrcmp(element, name) == 0)
        return i;
      i = i + 1;
    } while ((int)i < *(int *)((char *)param_1 + 0x204));
  }
  return -1;
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

  bsp_ref =
    (char *)tag_block_get_element(scenario_tag + 0x5a4, (int)bsp_index, 0x20);

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
      char *old_bsp = (char *)tag_block_get_element(
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
      (char *)tag_block_get_element(bsp_tag + 0xb0, 0, 0x60);
    *(char **)0x5064d8 =
      (char *)tag_block_get_element(bsp_tag + 0xb0, 0, 0x60);

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

/* 0x18ecd0 — unload the currently-loaded structure BSP if one is active.
 * If the cached BSP index (*(int16_t*)0x326a0c) differs from the index stored
 * at *(int16_t*)(*0x5064d0), frees the active BSP's structure-references block
 * element (scenario+0x5a4, size 0x20) via FUN_001ba0c0, marks the cached index
 * NONE, and re-switches to the index held at *0x5064d0. */
void FUN_0018ecd0(void)
{
  void *element;

  if (*(int16_t *)(*(char **)0x5064d0) != *(int16_t *)0x326a0c) {
    element = tag_block_get_element(*(char **)0x5064e4 + 0x5a4,
                                   (int)*(int16_t *)0x326a0c, 0x20);
    FUN_001ba0c0(element);
    *(int16_t *)0x326a0c = -1;
    scenario_switch_structure_bsp(*(int16_t *)(*(char **)0x5064d0));
  }
}

/* Load a scenario from the map cache. Opens the cache file, loads the
 * scenario and game globals tags, and switches to BSP 0. */
bool scenario_load(const char *map_name)
{
  int tag_index;
  int matg_index;
  char *scenario_tag;
  bool result = 0;

  ((void (*)(void *, const char *))0x8e770)((void *)0x326a6c, "scenario_load");
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

/* 0x18f230 — thin wrapper that forwards to scenario_location_from_point using
 * its 1st and 4th arguments (location_out=param_1, point=param_4); params 2
 * and 3 are received but unused. */
void FUN_0018f230(void *param_1, void *param_2, void *param_3, void *param_4)
{
  scenario_location_from_point(param_1, param_4);
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

/* Probes whether the global BSP3D leaf at the given position is solid (no leaf
   found). Steps the position up in Z by 0.05 up to 0x96 times until a leaf is
   located. Returns 1 if a leaf was found on the very first probe (position was
   immediately above geometry / "underwater"), 0 otherwise. */
char scenario_location_underwater(float *position)
{
  short count;
  uint32_t leaf;

  count = 0;
  while (1) {
    if (*(int *)0x5064d8 == 0) {
      display_assert("global_bsp3d",
                     "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xd5, 1);
      system_exit(-1);
    }
    leaf = bsp3d_find_leaf(*(void **)0x5064d8, 0, (void *)position);
    if (leaf != 0xffffffff) {
      break;
    }
    if (count >= 0x96) {
      break;
    }
    position[2] += 0.05f;
    count = count + 1;
  }
  return count == 0;
}

/* 0x18fef0 — reset a scenario module's global byte flag at 0x5057c0 to 0. */
void FUN_0018fef0(void)
{
  *(char *)0x5057c0 = 0;
}

/* 0x18d0b0 — per-frame coverage/big-sprite stats reset (kb name is a
 * misnomer). If the debug flag at 0x5064e8 is set, format the accumulated
 * coverage (float @0x506504) and big-sprite count (short @0x506508) into a
 * line and emit it via FUN_00189c40(0, ...). Then zero both accumulators and
 * run two matrix_transform_vector passes over the 0x5065b4 matrix using the
 * pointers at *0x31fc44 / *0x31fc40 into the 0x506510 / 0x50651c buffers.
 * cdecl, void(void). */
void scenario_fog_region_get_fog_index(void)
{
  char line[512];

  if (*(char *)0x5064e8 != 0) {
    crt_sprintf(line, "   coverage: %.1f big sprites: %d",
                (double)*(float *)0x506504,
                (int)*(short *)0x506508);
    FUN_00189c40(0, line);
  }
  *(float *)0x506504 = 0.0f;
  *(short *)0x506508 = 0;
  matrix_transform_vector((float *)0x5065b4, *(float **)0x31fc44,
                          (float *)0x506510);
  matrix_transform_vector((float *)0x5065b4, *(float **)0x31fc40,
                          (float *)0x50651c);
}

/* 0x18ed20 — find a scenario structure-BSP reference index by tag name.
 * Resolves param_2 (a tag index) to its name via tag_get_name, then linearly
 * scans the structure_bsp block (scenario_tag+0x5a4, 0x20-byte elements),
 * csstrcmp'ing each element's tag-name pointer (+0x14) against it. Returns the
 * 0-based index of the first match, or -1 if none. Count is re-read from
 * [param_1+0x5a4] each iteration, matching the original. cdecl; returns short
 * (only AX is written on the match path). */
short FUN_0018ed20(int param_1, int param_2)
{
  const char *target_name;
  char *element;
  short i;

  target_name = tag_get_name(param_2);
  for (i = 0; i < *(int *)(param_1 + 0x5a4); i++) {
    element = (char *)tag_block_get_element((void *)(param_1 + 0x5a4), i, 0x20);
    if (csstrcmp(target_name, *(const char **)(element + 0x14)) == 0) {
      return i;
    }
  }
  return -1;
}

/* 0x18d2c0 — render_sprite builder init: populate a sprite-build record
 * (param_1) from a shader/geometry source. Asserts the shader pointer is
 * non-NULL (render_sprite.c:0x14f) and that the _build_sprites_valid_bit
 * (flags & 4) is clear on entry (render_sprite.c:0x150). Stores fields then
 * snapshots the current view origin from *0x31fc1c (3 dwords) into +0x14..+0x1c,
 * and finally re-sets +0x10 to flags | _build_sprites_valid_bit. The +0x10
 * store happens twice (param_5 then param_5|4), matching the original. cdecl. */
void FUN_0018d2c0(uint32_t *param_1, int16_t param_2, uint32_t param_3,
                  int param_4, uint32_t param_5)
{
  uint32_t *view;

  if (param_4 == 0) {
    display_assert("shader", "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x14f,
                   1);
    system_exit(-1);
  }
  if ((param_5 & 4) != 0) {
    display_assert("!TEST_FLAG(flags, _build_sprites_valid_bit)",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x150, 1);
    system_exit(-1);
  }
  param_1[0] = param_3;          /* +0x00 */
  param_1[4] = param_5;          /* +0x10 (overwritten below) */
  param_1[2] = (uint32_t)param_4; /* +0x08 */
  *(int16_t *)((char *)param_1 + 0x20) = 0;
  *(int16_t *)((char *)param_1 + 0x0c) = 0;
  *(int16_t *)((char *)param_1 + 0x04) = param_2;
  view = *(uint32_t **)0x31fc1c;
  param_1[5] = view[0]; /* +0x14 */
  param_1[6] = view[1]; /* +0x18 */
  param_1[4] = param_5 | 4; /* +0x10 set _build_sprites_valid_bit */
  param_1[7] = view[2]; /* +0x1c */
}

/* 0x18df70 — triangle_strip iterator init (render/triangle_strips.c:0x16-0x17).
 * Asserts both the iterator (param_1) and the vertex-index buffer (param_2) are
 * non-NULL, then stores: +0x4 = index buffer ptr, +0x0 = vertex/index count
 * (int16 param_3), +0x2 = 0 (current position), +0x9 = 0x73 ('s' state tag).
 * cdecl, void. */
void FUN_0018df70(short *iterator, int index_buffer, short count)
{
  if (iterator == (short *)0) {
    display_assert("iterator", "c:\\halo\\SOURCE\\render\\triangle_strips.c",
                   0x16, 1);
    system_exit(-1);
  }
  if (index_buffer == 0) {
    display_assert("triangle_strip_vertex_indices",
                   "c:\\halo\\SOURCE\\render\\triangle_strips.c", 0x17, 1);
    system_exit(-1);
  }
  *(int *)(iterator + 2) = index_buffer;
  *iterator = count;
  iterator[1] = 0;
  *((char *)iterator + 9) = 0x73;
}

/* 0x18e800 — scenario_ensure_point_within_world: test whether a cluster is
 * audible/visible by checking bit cluster_index1 in the current structure
 * BSP's per-cluster sound-data bitvector for source cluster_index. Asserts the
 * BSP is loaded (*0x5064e0, scenario.c:0xc5) and that cluster_index1 is in
 * range [0, structure_bsp->clusters.count) (bsp+0x134, scenario.c:0x1d4).
 * Returns the tested bit as 0/1. cdecl: cluster_index [EBP+8] (int, passed as
 * the callee's int16_t arg), cluster_index1 [EBP+0xc] (MOVSX int16_t). */
bool scenario_ensure_point_within_world(int cluster_index,
                                        int16_t cluster_index1)
{
  void *bsp;
  uint32_t *sound_data;

  if (*(int *)0x5064e0 == 0) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }
  bsp = *(void **)0x5064e0;
  sound_data = structure_bsp_get_cluster_sound_data(bsp, (int16_t)cluster_index);
  if (cluster_index1 < 0 || (int)cluster_index1 >= *(int *)((char *)bsp + 0x134)) {
    display_assert(
        "cluster_index1>=0 && cluster_index1<structure_bsp->clusters.count",
        "c:\\halo\\SOURCE\\scenario\\scenario.c", 0x1d4, 1);
    system_exit(-1);
  }
  return (sound_data[(int)cluster_index1 >> 5] & (1u << ((int)cluster_index1 & 0x1f))) != 0;
}

/* 0x18ef00 — test an object's physics position against cluster volume
 * cluster_index. Resolves the object handle (no type restriction) and forwards
 * the object's physics-position vector (object_data + 0x50) to the cluster
 * point-in-volume test FUN_0018ed90. Returns false (AL=0) when the handle is
 * -1. cdecl: cluster_index [EBP+8], object_handle [EBP+0xc]; byte (AL) return. */
char FUN_0018ef00(int cluster_index, int object_handle)
{
  char result;
  int obj;

  result = 0;
  if (object_handle != -1) {
    obj = (int)object_get_and_verify_type(object_handle, 0xffffffff);
    result = FUN_0018ed90((short)cluster_index, (float *)(obj + 0x50));
  }
  return result;
}

/* 0x18e140 — look up the multiplayer scenario-description tag and return its
 * scenario count. Asserts the out-param is non-NULL, then resolves the
 * 'mply' tag "ui\multiplayer_scenarios". When loaded, writes the tag's int16
 * at +0x0 into *out_short and returns the int32 count at +0x4. When the tag
 * is not loaded, writes 0 to *out_short and returns 0. cdecl: out_short
 * [EBP+8] (-> EDI). */
int FUN_0018e140(short *out_short)
{
  int count;
  void *tag;

  if (out_short == 0) {
    display_assert("count",
        "c:\\halo\\SOURCE\\scenario\\multiplayer_scenario_description.c", 0x3d, 1);
    system_exit(-1);
  }
  count = tag_loaded(0x6d706c79, "ui\\multiplayer_scenarios");
  if (count != -1) {
    tag = tag_get(0x6d706c79, count);
    if (tag == 0) {
      display_assert("scenario_list",
          "c:\\halo\\SOURCE\\scenario\\multiplayer_scenario_description.c", 0x43, 1);
      system_exit(-1);
    }
    count = *(int *)((char *)tag + 4);
    *out_short = *(short *)tag;
    return count;
  }
  *out_short = 0;
  return 0;
}

/* 0x18e6a0 — copy three world basis/origin vectors into up to four caller
 * buffers. The globals are POINTERS to 3-float vectors (double-deref): up
 * vector (*0x31fc44), left vector (*0x31fc40), and a third vector (*0x2ee708)
 * delivered to BOTH out_d and out_e. Each out-param is optional (skipped when
 * NULL). Always returns 1. param_1 [EBP+8] is unused. cdecl. */
char FUN_0018e6a0(int unused, float *out_up, float *out_left, float *out_d,
                  float *out_e)
{
  float *src;

  src = *(float **)0x0031fc44;
  if (out_up != 0) {
    out_up[0] = src[0];
    out_up[1] = src[1];
    out_up[2] = src[2];
  }
  src = *(float **)0x0031fc40;
  if (out_left != 0) {
    out_left[0] = src[0];
    out_left[1] = src[1];
    out_left[2] = src[2];
  }
  src = *(float **)0x002ee708;
  if (out_d != 0) {
    out_d[0] = src[0];
    out_d[1] = src[1];
    out_d[2] = src[2];
  }
  src = *(float **)0x002ee708;
  if (out_e != 0) {
    out_e[0] = src[0];
    out_e[1] = src[1];
    out_e[2] = src[2];
  }
  return 1;
}

/* 0x18e8a0 — AND the two clusters' PVS (potentially-visible-set) bit vectors.
 * Asserts the structure BSP global (0x5064e0) is loaded, fetches each region's
 * cluster sound-data PVS bitfield via structure_bsp_get_cluster_sound_data,
 * then bit_vector_and's them, size = cluster count (uint16 @bsp+0x134), into a
 * NULL result buffer (in-place / dry-run, result discarded). cdecl, void. */
void scenario_get_fog_region_index(int region_a, int region_b)
{
  int bsp;
  int pvs_a;
  int pvs_b;

  if (*(int *)0x5064e0 == 0) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }
  bsp = *(int *)0x5064e0;
  pvs_a = (int)structure_bsp_get_cluster_sound_data((void *)bsp, region_a);
  pvs_b = (int)structure_bsp_get_cluster_sound_data((void *)bsp, region_b);
  bit_vector_and(*(unsigned short *)(bsp + 0x134), pvs_a, pvs_b, 0);
}

/* 0x18c0b0 — fetch an object's cached cluster-lighting datum data pointer.
 * Resolves the object's cluster-lighting record index via FUN_0018bf80
 * (object_handle, lod). On success returns the datum's data + 0x14 from the
 * cluster-lighting pool (*(data_t**)0x50652c). On the NONE (-1) path, falls
 * back to the global lighting-state buffer at 0x4d8258: rebuilds its baseline
 * lighting (FUN_0013bce0) and its dynamic light markers (FUN_0013aa10), then
 * returns &DAT_004d8258. cdecl: object_handle [EBP+8], lod [EBP+0xc] (float,
 * forwarded bitwise to FUN_0018bf80). NOTE: kb name 'scenario_leaf_index_from
 * _point' appears to be a misnomer (object-handle arg, pointer return). */
void *scenario_leaf_index_from_point(int object_handle, float lod)
{
  int index;

  index = FUN_0018bf80(object_handle, lod);
  if (index != -1) {
    return (void *)((int)datum_get(*(data_t **)0x50652c, index) + 0x14);
  }
  FUN_0013bce0(object_handle, (float *)0x4d8258);
  FUN_0013aa10(object_handle, 0x4d8258);
  return (void *)0x4d8258;
}

/* 0x18c3a0 — scenario_test_pvs: per-frame potentially-visible-set debug draw.
 * Optionally brackets the work in a 'render_objects' profile section (when both
 * the global debug-enable byte 0x449ef1 and the section-enable byte 0x325810
 * are set). Resets the sprite/decal draw state (FUN_0017d1a0(0)) and primes the
 * object iteration list (FUN_0018b080), then runs the body loop exactly twice:
 * the first pass (BL toggle 0->1) and the second. On the pass whose toggle byte
 * BL equals the global char at 0x3256c6 it walks the visible-object table at
 * 0x4d82d4 (count is the signed short at 0x4d82d0), seeding a 0x48-byte stack
 * record per entry (record[0] = table[i]) and processing it via FUN_0018c100
 * (record passed in EDI); on the other pass it draws first-person weapons
 * (first_person_weapon_draw). Finishes by flushing geometry (FUN_0017cbf0).
 * void(void). */
void scenario_test_pvs(void)
{
  char pass;
  short i;
  bool again;
  char record[0x48];

  if (*(char *)0x449ef1 != 0 && *(char *)0x325810 != 0) {
    profile_enter_private((void *)0x325808);
  }
  FUN_0017d1a0(0);
  FUN_0018b080();
  pass = 0;
  record[8] = 0;
  do {
    if (pass == *(char *)0x3256c6) {
      i = 0;
      if (0 < *(short *)0x4d82d0) {
        do {
          *(int *)record = *(int *)(0x4d82d4 + (int)i * 4);
          FUN_0018c100(record);
          i = i + 1;
        } while (i < *(short *)0x4d82d0);
      }
    } else {
      first_person_weapon_draw();
    }
    again = (pass == 0);
    pass = 1;
  } while (again);
  FUN_0017cbf0();
  if (*(char *)0x449ef1 != 0 && *(char *)0x325810 != 0) {
    profile_exit_private((void *)0x325808);
  }
}

/* 0x18c460 — scenario_test_pas: per-frame potentially-audible-set debug draw.
 * Optionally brackets the work in a 'render_object_shadows' profile section
 * (when both the global debug-enable byte 0x449ef1 and the section-enable byte
 * 0x325e08 are set). When the master enable byte 0x325800 is set, it brackets
 * a sky/object iteration pass between two decal helpers (FUN_0017cca0 ...
 * FUN_0017cd10): a 0x48-byte stack record is primed (record[8] = 1) and passed
 * in EAX to scenario_get_sky, which drives the per-object iteration loop and
 * processing internally. void(void). */
void scenario_test_pas(void)
{
  char record[0x48];

  if (*(char *)0x449ef1 != 0 && *(char *)0x325e08 != 0) {
    profile_enter_private((void *)0x325e00);
  }
  if (*(char *)0x325800 != 0) {
    FUN_0017cca0();
    record[8] = 1;
    scenario_get_sky(record);
    FUN_0017cd10();
  }
  if (*(char *)0x449ef1 != 0 && *(char *)0x325e08 != 0) {
    profile_exit_private((void *)0x325e00);
  }
}

/* 0x18e1d0 — scenario_debug_to_file: split a scenario's name (at scenario+0x20)
 * on its last backslash and format "<name>\<tail>" into the caller's output
 * buffer. Asserts the scenario pointer (param_1) is non-NULL. Walks backward
 * from the end of the name (strlen via csstrlen) to the last '\\'; if none is
 * found returns 1 with the buffer left empty. On success formats via snprintf
 * (out, size, "%s\\%s", name, tail) where tail = name + (offset_of_backslash +
 * 1). Always returns 1. cdecl: scenario [EBP+8], out_buf [EBP+0xc], buf_size
 * [EBP+0x10]. From multiplayer_scenario_description.c:0x59. */
int scenario_debug_to_file(int param_1, char *param_2, unsigned int param_3)
{
  int name;
  int idx;

  if (param_1 == 0) {
    display_assert("item",
                   "c:\\halo\\SOURCE\\scenario\\multiplayer_scenario_description.c",
                   0x59, 1);
    system_exit(-1);
  }
  name = param_1 + 0x20;
  *param_2 = 0;
  idx = csstrlen((const char *)name);
  if (-1 < idx) {
    while (*(char *)(name + idx) != '\\') {
      idx = idx - 1;
      if (idx < 0) {
        return 1;
      }
    }
    snprintf(param_2, param_3, "%s\\%s", (char *)name,
             (char *)(idx + 0x21 + param_1));
  }
  return 1;
}

/* 0x18bf80 — resolve/allocate the cached object render-state datum index for
 * an object. The object stores its render-state index at object_data+0x120.
 *   1. If that index is valid (!= -1) and the pooled datum at that index still
 *      references this object (datum+0x4 == object_handle) and the object's
 *      stored index is still valid, refresh that render-state (FUN_0018bc60,
 *      force_rebuild=0) and return the existing index.
 *   2. Otherwise allocate a new datum slot (data_new_at_index). If the pool is
 *      full (-1), evict the entry with the greatest "age" score: for each active
 *      datum, age = (float)(DAT_00506544 - datum+0xc); a negative age is clamped
 *      to 1000.0f (0x254cb8). The slot with the maximum age wins; start the
 *      search at -FLT_MAX (0xff7fffff). If nothing is found, return -1.
 *   3. Build the chosen render-state (FUN_0018bc60, force_rebuild=1) and store
 *      the index at object_data+0x120.
 * cdecl: object_handle [EBP+8], lod [EBP+0xc] (float, forwarded bitwise to
 * FUN_0018bc60). Pool is *(data_t**)0x50652c (cached object render states).
 * Confirmed: 0x2533c0 = 0.0f, 0x254cb8 = 1000.0f, -FLT_MAX init = 0xff7fffff. */
int FUN_0018bf80(int object_handle, float lod)
{
  char *obj;
  int existing_index;
  char *datum;
  int new_index;
  int iter_index;
  float best_age;
  float age;

  obj = (char *)object_get_and_verify_type(object_handle, 0xffffffff);
  existing_index = *(int *)(obj + 0x120);
  if (existing_index != -1) {
    datum = (char *)datum_get(*(data_t **)0x50652c, existing_index);
    if (*(int *)(datum + 4) == object_handle) {
      existing_index = *(int *)(obj + 0x120);
      if (existing_index != -1) {
        FUN_0018bc60(object_handle, lod, 0);
        return existing_index;
      }
    }
  }

  new_index = data_new_at_index(*(data_t **)0x50652c);
  if (new_index == -1) {
    best_age = -3.4028235e+38f;
    for (iter_index = data_next_index(*(data_t **)0x50652c, 0xffffffff);
         iter_index != -1;
         iter_index = data_next_index(*(data_t **)0x50652c, iter_index)) {
      datum = (char *)datum_get(*(data_t **)0x50652c, iter_index);
      age = (float)(*(int *)0x506544 - *(int *)(datum + 0xc));
      if (age < *(float *)0x2533c0) {
        age = *(float *)0x254cb8;
      }
      if (best_age < age) {
        new_index = iter_index;
        best_age = age;
      }
    }
    if (new_index == -1) {
      return -1;
    }
  }

  FUN_0018bc60(object_handle, lod, 1);
  *(int *)(obj + 0x120) = new_index;
  return new_index;
}

/* 0x18ed90 — test whether a world-space point lies inside a scenario cluster's
 * shape volume. The cluster element (0x60 bytes) comes from the global scenario
 * tag's cluster block (global_scenario + 0x360, element size 0x60), indexed by
 * cluster_index. The shape type is the int16 at offset +0:
 *   - type 0 (axis-aligned box): the point is inside iff each component lies
 *     strictly within the half-open bounds box[+0x48..+0x4c] (x),
 *     box[+0x50..+0x54] (y), box[+0x58..+0x5c] (z); the low bound uses '<='
 *     (must be strictly greater) and the high bound uses '>=' (must be strictly
 *     less). Any out-of-range component returns 0.
 *   - type 1 (oriented box): build a 4x3 transform from the box basis
 *     (position box+0x48, forward box+0x30, up box+0x3c), transform the point
 *     into box-local space, then require each local component to be in
 *     (0, box+0x54), (0, box+0x58), (0, box+0x5c) respectively.
 * Any other type asserts. Returns 1 if inside, 0 otherwise.
 * cdecl: cluster_index [EBP+8] (int16, MOVSX), point [EBP+0xc] (float*). */
char FUN_0018ed90(short cluster_index, float *point)
{
  char *box;
  float local[16];
  float transformed[3];
  float z;

  if (*(int *)0x5064e4 == 0) {
    display_assert("global_scenario",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xb7, 1);
    system_exit(-1);
  }

  box = (char *)tag_block_get_element(
    (void *)(*(int *)0x5064e4 + 0x360), (int)cluster_index, 0x60);

  switch (*(int16_t *)box) {
  case 0:
    if (point[0] <= *(float *)(box + 0x48)) {
      return 0;
    }
    if (point[1] <= *(float *)(box + 0x50)) {
      return 0;
    }
    if (point[2] <= *(float *)(box + 0x58)) {
      return 0;
    }
    if (!(point[0] < *(float *)(box + 0x4c))) {
      return 0;
    }
    if (!(point[1] < *(float *)(box + 0x54))) {
      return 0;
    }
    z = point[2];
    break;
  case 1:
    matrix4x3_from_forward_up_position(
      local, (float *)(box + 0x48), (float *)(box + 0x30),
      (float *)(box + 0x3c));
    real_matrix3x3_transform_point(local, point, transformed);
    if (transformed[0] <= *(float *)0x2533c0) {
      return 0;
    }
    if (transformed[1] <= *(float *)0x2533c0) {
      return 0;
    }
    if (transformed[2] <= *(float *)0x2533c0) {
      return 0;
    }
    if (!(transformed[0] < *(float *)(box + 0x54))) {
      return 0;
    }
    if (!(transformed[1] < *(float *)(box + 0x58))) {
      return 0;
    }
    z = transformed[2];
    break;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\scenario\\scenario.c", 0x331, 1);
    system_exit(-1);
    z = 0.0f;
    break;
  }

  if (!(z < *(float *)(box + 0x5c))) {
    return 0;
  }
  return 1;
}

/* 0x18b010 — predicate: should the given object be treated as the local
 * player's first-person view subject? Resolves the local player's player
 * index (local_player_get_player_index of the local-player index global at
 * 0x506548). If valid, fetches that player's controlled unit handle from the
 * players data pool (*(int*)0x5aa6d4) at player_data+0x34. The object qualifies
 * (returns 1) when it is that unit AND the director perspective for the local
 * player (director_get_perspective) is 0 (first-person). Otherwise it falls
 * back to FUN_00085150(object_handle) (observer/cinematic check) and returns 1
 * if that is set, else 0.
 * ABI: object_handle in ESI (@<esi>), frameless. Returns int bool. Sole caller
 * FUN_0018c100 sets ESI = *buf (the object handle). */
int FUN_0018b010(int object_handle)
{
  int unit_handle;

  if (local_player_get_player_index(*(int16_t *)0x506548) == -1) {
    unit_handle = -1;
  } else {
    unit_handle = *(int *)((int)datum_get(
      *(data_t **)0x5aa6d4,
      local_player_get_player_index(*(int16_t *)0x506548)) + 0x34);
  }

  if (unit_handle == object_handle &&
      director_get_perspective(*(int16_t *)0x506548) == 0) {
    return 1;
  }

  if (FUN_00085150(object_handle)) {
    return 1;
  }
  return 0;
}

/* 0x18e9b0 — test whether a BSP location's cluster is in the combined
 * potentially-visible set for all players. Asserts the location's cluster_index
 * (location+4, int16) is in [0, global_structure_bsp->clusters.count). Then
 * fetches the combined PVS bit vector (players_get_combined_pvs) and returns
 * the cluster's bit. This is the all-players variant of
 * scenario_location_potentially_visible_local (which uses the local-player PVS);
 * its assert line is 0x1ef vs 0x1e7. cdecl: location [EBP+8]. Returns bool. */
bool scenario_location_potentially_visible(void *location)
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
    "c:\\halo\\SOURCE\\scenario\\scenario.c", 0x1ef, 1);
  system_exit(-1);

valid:
  cluster_index = *(int16_t *)((char *)location + 4);
  pvs = players_get_combined_pvs();
  return (*(uint32_t *)((char *)pvs + ((int)cluster_index >> 5) * 4) &
          (1u << (cluster_index & 0x1f))) != 0;
}

/* 0x18e5c0 — does the BSP location's cluster have a "stop" background-sound
 * (sound environment) flag set? Looks up the cluster element
 * (global_structure_bsp + 0x134, element size 0x68) by location->cluster_index
 * (location+4), reads the cluster's background-sound reference (cluster+4,
 * int16). If valid (!= -1) and in range of the BSP background-sound block
 * (bsp + 0x1fc, element size 0x74), reads that block element's sound tag index
 * (+0x2c); if valid, resolves the 'lsnd' tag (tag_get 0x6c736e64) and returns
 * its first byte's bit 0. Returns 0 on any missing reference.
 * cdecl: location [EBP+8]. Returns char (bool). */
char FUN_0018e5c0(int location)
{
  char *cluster;
  char *sound_element;
  int tag_index;
  char result;

  if (!global_structure_bsp) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  cluster = (char *)tag_block_get_element(
    (char *)global_structure_bsp + 0x134,
    (int)*(int16_t *)(location + 4), 0x68);

  result = 0;
  if (*(int16_t *)(cluster + 4) != -1) {
    if (!global_structure_bsp) {
      display_assert("global_structure_bsp",
                     "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
      system_exit(-1);
    }

    if ((int)*(int16_t *)(cluster + 4) <
        *(int *)((char *)global_structure_bsp + 0x1fc)) {
      sound_element = (char *)tag_block_get_element(
        (char *)global_structure_bsp + 0x1fc, (int)*(int16_t *)(cluster + 4),
        0x74);
      tag_index = *(int *)(sound_element + 0x2c);
      if (tag_index != -1) {
        return *(char *)tag_get(0x6c736e64 /* 'lsnd' */, tag_index) & 1;
      }
    }
  }
  return result;
}

/* 0x18d670 — projection-cosine helper (pure FPU leaf, no callees). For mode 0
 * returns 1.0f. For non-zero mode, returns the magnitude of the normalized
 * projection of v2 onto v1's direction: |dot(v1, v2) / |v1||. For mode 2 the
 * result is complemented as (1.0f - that). Used to score how aligned two
 * direction vectors are. NOTE: the kb name "scenario_reload_structure_bsp_if_
 * necessary" is a mis-attribution; this is a vector-angle helper.
 * cdecl: mode [EBP+8] (int16), v1 [EBP+0xc] (float*), v2 [EBP+0x10] (float*).
 * Returns float. Confirmed: 0x2533c8 = 1.0f; FSQRT over v1 only; FABS; FSUBR
 * for the mode-2 complement. */
float FUN_0018d670(short mode, float *v1, float *v2)
{
  float result;

  result = *(float *)0x2533c8;
  if (mode != 0) {
#if defined(_MSC_VER) && !defined(__clang__)
    /* VC71 /Oi inlines fabs((double)x) as the x87 FABS instruction, matching
     * the original's inline FABS on ST. clang (shipped build) takes the #else
     * branch below, so the binary is unchanged. Analog of lift-score-improve
     * technique 1 (cos/sin intrinsification) applied to fabsf. */
    result = (float)fabs((double)((v1[0] * v2[0] + v2[1] * v1[1] + v2[2] * v1[2]) /
                   sqrtf(v1[2] * v1[2] + v1[1] * v1[1] + v1[0] * v1[0])));
#else
    result = fabsf((v1[0] * v2[0] + v2[1] * v1[1] + v2[2] * v1[2]) /
                   sqrtf(v1[2] * v1[2] + v1[1] * v1[1] + v1[0] * v1[0]));
#endif
    if (mode == 2) {
      result = *(float *)0x2533c8 - result;
    }
  }
  return result;
}

