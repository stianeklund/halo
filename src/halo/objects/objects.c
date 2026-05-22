/*
 * real_vector3d_valid — check whether a 3D vector contains only finite floats.
 *
 * Tests the IEEE 754 exponent field (bits 23..30) of each of the three
 * components. If all three have an exponent != 0xFF (i.e. the value is
 * neither NaN nor Infinity), returns 1 (valid). Otherwise returns 0.
 *
 * Leaf function, no callees. Reinterprets floats as uint32 via pointer cast.
 *
 * Confirmed: AND with 0x7f800000 and CMP to 0x7f800000 for each component.
 * Confirmed: returns 1 only if all three pass; returns 0 on first failure.
 */
/* 0x84a10 */
int real_vector3d_valid(float *vector)
{
  unsigned int *v = (unsigned int *)vector;
  if ((v[0] & 0x7f800000) == 0x7f800000)
    return 0;
  if ((v[1] & 0x7f800000) == 0x7f800000)
    return 0;
  if ((v[2] & 0x7f800000) == 0x7f800000)
    return 0;
  return 1;
}

/* 0x84a70 — valid_real_normal3d_perpendicular: check whether two 3D vectors
 * are each valid unit normals AND are perpendicular to each other.
 *
 * First validates each vector individually via valid_real_normal3d (checks
 * that squared length is within 0.001 of 1.0 and not NaN/infinity).
 * Then computes dot(a, b) and returns true only if the dot product is
 * a valid finite float with fabsf(dot) < 0.001f (i.e., nearly perpendicular).
 *
 * Confirmed: CALL 0x21fb0 twice (valid_real_normal3d) for each input vector.
 * Confirmed: FLD/FMUL/FADDP computes dot(a, b) = a[0]*b[0]+a[1]*b[1]+a[2]*b[2].
 * Confirmed: FSTS [EBP-4] stores dot without popping; integer NaN/inf check
 *   on exponent bits (AND 0x7f800000, CMP 0x7f800000) before the FABS compare.
 * Confirmed: FABS / FCOMPL double ptr [0x2549d8] compares against
 * (double)0.001f. Confirmed: FNSTSW AX / TEST AH,5 / JP pattern — returns true
 * when fabsf(dot) < 0.001f. */
int valid_real_normal3d_perpendicular(float *a, float *b)
{
  float dot;

  if (!valid_real_normal3d(a)) {
    return 0;
  }
  if (!valid_real_normal3d(b)) {
    return 0;
  }

  dot = a[2] * b[2] + a[1] * b[1] + a[0] * b[0];

  if ((*(unsigned int *)&dot & 0x7f800000) == 0x7f800000) {
    return 0;
  }

  return fabsf(dot) < 0.001f;
}

/*
 * FUN_000ae0a0 — game-engine tag-index remapping dispatch.
 *
 * When a game engine is active (*(int *)0x456b60 != 0) and tag_index is
 * valid (!= -1), reads the object type word from the 'obje' tag header
 * (first 2 bytes) and dispatches to the appropriate engine-specific
 * tag-remapping helper:
 *   type 1 (vehicle) → game_engine_remap_vehicle(tag_index)
 *   type 2 (weapon)  → game_engine_remap_weapon(tag_index)
 *   type 3 (?)       → FUN_000adf70(tag_index)
 * Returns the (possibly remapped) tag index, or the original tag_index
 * if no engine is active, tag_index is -1, or the type is not 1/2/3.
 *
 * Confirmed: MOV EAX,[0x456b60] / TEST EAX,EAX — bool check on engine ptr.
 * Confirmed: PUSH ESI / PUSH 0x6f626a65 / CALL tag_get — 'obje' tag lookup.
 * Confirmed: MOV AX,[EAX] — first 2 bytes of tag data = object type word.
 * Confirmed: CMP AX,1 / CMP AX,2 / CMP AX,3 — three dispatch branches.
 * Confirmed: each branch PUSH ESI / CALL callee / ADD ESP,4; returns EAX.
 * Confirmed: fallthrough MOV EAX,ESI — returns original tag_index unchanged.
 * Note: callee decls carry wrong void return type; binary shows they return
 * int.
 */
int FUN_000ae0a0(int tag_index)
{
  short obj_type;
  short *tag_data;

  if ((*(int *)0x456b60 != 0) && (tag_index != -1)) {
    tag_data = (short *)tag_get(0x6f626a65, tag_index);
    obj_type = *tag_data;
    if (obj_type == 1) {
      return game_engine_remap_vehicle(tag_index);
    }
    if (obj_type == 2) {
      return game_engine_remap_weapon(tag_index);
    }
    if (obj_type == 3) {
      return FUN_000adf70(tag_index);
    }
  }
  return tag_index;
}

/* FUN_00136150 — create widgets for an object from its tag definition.
 *
 * Looks up the object's tag (group 'obje'), reads the widget attachments
 * tag block at tag+0x14c, and for each attachment, searches the global
 * widget_types table (5 entries at 0x323528, each 0x28 bytes) for a
 * matching group_tag. When found, allocates a new widget datum from the
 * widget data pool at 0x5a90c4, sets its type field, and either:
 *   - calls the widget type's "new" function (entry+0x18) with the
 *     attachment's definition index (element+0x0c), linking on success
 *   - or directly links the widget with definition_handle = -1 if no
 *     "new" function is defined.
 * Widgets are prepended to a singly-linked list rooted at obj+0x11c.
 *
 * Source: c:\halo\source\objects\widgets\widget_types.h (line 0x96)
 *
 * Confirmed: 1 cdecl arg (object_handle).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0x1ba140 (tag_get) with (0x6f626a65, obj[0]).
 * Confirmed: CALL 0x19b210 (tag_block_get_element) with (block, index, 0x20).
 * Confirmed: CALL 0x119610 (data_new_at_index) with (*(data_t**)0x5a90c4).
 * Confirmed: CALL 0x119320 (datum_get) with (*(data_t**)0x5a90c4, handle).
 * Confirmed: CALL 0x1196d0 (datum_delete) with (*(data_t**)0x5a90c4, handle).
 * Confirmed: widget_types table at 0x323528: [+0x00]=group_tag, [+0x18]=new_fn.
 * Confirmed: ADD ESP,0x10 cleans both object_get_and_verify_type + tag_get
 * pushes. Confirmed: outer loop counter is int16_t (MOVSX EAX,AX at 0x1362b2).
 * Confirmed: inner loop counter is int16_t (MOVSX ECX,SI; CMP SI,0x5).
 * Confirmed: indirect CALL EAX at 0x13625a for widget new function.
 * Confirmed: assert_halt for type range check at 0x1361fe.
 */

/* Allocates a new entry in the 0x46f020 data table and stores param_1 at +4.
 * Returns the datum handle, or -1 on failure.
 * 0x134be0 / objects.obj
 */
int FUN_00134be0(int param_1)
{
  int iVar1;
  int iVar2;

  iVar1 = data_new_at_index(*(data_t **)0x46f020);
  if (iVar1 != -1) {
    iVar2 = (int)datum_get(*(data_t **)0x46f020, iVar1);
    *(int *)(iVar2 + 4) = param_1;
  }
  return iVar1;
}

/* Deletes the entry at param_1 from the 0x46f020 data table.
 * 0x134c20 / objects.obj
 */
void FUN_00134c20(int param_1)
{
  if (param_1 != -1) {
    datum_delete(*(data_t **)0x46f020, param_1);
  }
}

/* Allocates a new entry in the 0x46f024 data table and stores param_1 at +4.
 * Returns the datum handle, or -1 on failure.
 * 0x1353b0 / objects.obj
 */
int FUN_001353b0(int param_1)
{
  int iVar1;
  int iVar2;

  iVar1 = data_new_at_index(*(data_t **)0x46f024);
  if (iVar1 != -1) {
    iVar2 = (int)datum_get(*(data_t **)0x46f024, iVar1);
    *(int *)(iVar2 + 4) = param_1;
  }
  return iVar1;
}

/* Deletes the entry at param_1 from the 0x46f024 data table.
 * 0x1353f0 / objects.obj
 */
void FUN_001353f0(int param_1)
{
  if (param_1 != -1) {
    datum_delete(*(data_t **)0x46f024, param_1);
  }
}

int FUN_0009ec30(int effect_index, int object_handle, int parent_handle,
                 int marker, int arg4, int arg5, int arg6, int arg7);

/*
 * objects/objects.c — object system lifecycle and placement
 * XBE source: c:\halo\SOURCE\objects\objects.c
 *            + c:\halo\SOURCE\objects\object_lights.c (same .obj)
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x1396e0  object_wake (object_lights.c)
 *   0x13aed0  object_move_to_limbo (object_lights.c)
 *   0x13d640  object_try_and_get_and_verify_type
 *   0x13d680  object_get_and_verify_type
 *   0x13d920  object_set_garbage_flag
 *   0x13dfc0  object_header_block_reference_get
 *   0x13e510  object_child_list_remove
 *   0x13eb70  object_reset_markers
 *   0x13ee60  object_propagate_flag_to_children
 *   0x13eff0  object_remove_from_name_list
 *   0x13f060  objects_place
 *   0x13f810  objects_initialize
 *   0x13f950  objects_initialize_for_new_map
 *   0x13f9f0  objects_dispose_from_old_map
 *   0x13fac0  objects_dispose
 *   0x13fb30  object_activate
 *   0x13fb80  object_deactivate (object deactivate)
 *   0x13fc20  object_placement_data_new (object placement data init)
 *   0x13fd00  object_disconnect_from_map
 *   0x13fef0  object_has_node
 *   0x13ff50  object_set_automatic_deactivation (object set/clear hidden)
 *   0x13ffc0  object_set_garbage
 *   0x140160  object_set_region_count
 *   0x140230  object_adjust_interpolation_position
 *   0x140420  object_find_in_cluster
 *   0x1407e0  object_visible_to_any_player
 *   0x140bc0  object_delete_internal
 *   0x140cc0  object_delete
 *   0x140ce0  object_connect_to_map
 *   0x140eb0  object_get_node_matrix
 *   0x140f10  object_get_markers_by_string_id
 *   0x141020  object_compute_child_marker_position
 *   0x1412f0  object_get_world_position
 *   0x141360  object_get_orientation (object orientation getter)
 *   0x141480  object_get_world_matrix
 *   0x1415f0  object_find_in_radius
 *   0x141b70  object_compute_node_matrices
 *   0x143ae0  object_set_position (object reposition)
 *   0x143be0  object_translate (set object position and reconnect to map)
 *   0x143c80  object_new (object_new — create from placement)
 *   0x144240  object_attach_to_parent
 *   0x1446a0  object_update_children_recursive
 *   0x144860  object_attach_to_marker
 *   0x144b30  objects_garbage_collection (delete and immediately deactivate)
 *   0x145170  objects_update
 */

#include "common.h"

/* Forward declarations for unported callees in the same .obj cluster. */
typedef void (*pfn_void_t)(void);
typedef void (*pfn_int_t)(int);
typedef int (*valid_real_point3d_fn)(float *p);
typedef void (*object_type_validate_fn)(int16_t type);

/* game engine tag-index remapping helpers (called from FUN_000ae0a0).
 * Binary: each takes 1 cdecl int arg, returns int in EAX. */
int game_engine_remap_vehicle(int tag_index);
int game_engine_remap_weapon(int tag_index);
int FUN_000adf70(int tag_index);

/*
 * object_set_position — reposition an object and recompute its orientation.
 *
 * Disconnects the object from the map, optionally updates its position
 * (forward vector at obj+0x0C) and facing direction (at obj+0x24).
 * If a target (up) vector is provided, it is copied directly to obj+0x30.
 * Otherwise, a perpendicular up vector is computed from the facing via:
 *   temp = {facing.y, -facing.x, 0.0}
 *   normalize(temp)
 *   if degenerate: temp = {1, 0, 0}
 *   up = cross(temp, facing)
 * Then recomputes node matrices and reconnects to the map.
 *
 * Confirmed: 4 cdecl args (object_handle, facing, target, flags).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0x13fd00 (object_disconnect_from_map) with 1 stack arg.
 * Confirmed: CALL 0x13010 (normalize3d) for perpendicular temp vector.
 * Confirmed: cross product computed via x87 FPU in-line (not a function call).
 * Confirmed: CALL 0x141b70 (object_compute_node_matrices).
 * Confirmed: CALL 0x140ce0 (object_connect_to_map) with (handle, 0).
 * Confirmed: FCOMP against *(float*)0x2533c0 (0.0f) for degenerate check.
 */
/* Find widget type index by group tag. Returns 0-4 on match, 0xffff if not
 * found. 0x135f20 / objects.obj
 */
/* Allocate widget data pool, then call each widget type's initialize function.
 * 0x135f90 / objects.obj
 */
void FUN_00135f90(void)
{
  short sVar1;
  void **ppuVar2;

  *(data_t **)0x5a90c4 = game_state_data_new("widget", 0x40, 0xc);
  if (*(data_t **)0x5a90c4 == 0) {
    display_assert("widget_data",
                   "c:\\halo\\SOURCE\\objects\\widgets\\widgets.c", 0x2e, 1);
    system_exit(-1);
  }
  sVar1 = 0;
  ppuVar2 = (void **)0x323530;
  do {
    if ((sVar1 < 0) || (4 < sVar1)) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    if (ppuVar2[-2] == 0) {
      display_assert("type_definition->group_tag",
                     "c:\\halo\\SOURCE\\objects\\widgets\\widgets.c", 0x37, 1);
      system_exit(-1);
    }
    if (*ppuVar2 != 0) {
      ((void (*)(void)) * ppuVar2)();
    }
    sVar1 = sVar1 + 1;
    ppuVar2 = ppuVar2 + 10;
  } while (sVar1 < 5);
}

/* Reset widget data pool, then call each widget type's initialize_for_new_map.
 * 0x136040 / objects.obj
 */
void FUN_00136040(void)
{
  short sVar1;
  void **ppuVar2;

  data_delete_all(*(data_t **)0x5a90c4);
  sVar1 = 0;
  ppuVar2 = (void **)0x323534;
  do {
    if ((sVar1 < 0) || (4 < sVar1)) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    if (*ppuVar2 != 0) {
      ((void (*)(void)) * ppuVar2)();
    }
    sVar1 = sVar1 + 1;
    ppuVar2 = ppuVar2 + 10;
  } while (sVar1 < 5);
}

/* Call each widget type's dispose_from_old_map, then invalidate widget data
 * pool. 0x1360a0 / objects.obj
 */
void FUN_001360a0(void)
{
  short sVar1;
  void **ppuVar2;

  sVar1 = 0;
  ppuVar2 = (void **)0x323538;
  do {
    if ((sVar1 < 0) || (4 < sVar1)) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    if (*ppuVar2 != 0) {
      ((void (*)(void)) * ppuVar2)();
    }
    sVar1 = sVar1 + 1;
    ppuVar2 = ppuVar2 + 10;
  } while (sVar1 < 5);
  data_make_invalid(*(data_t **)0x5a90c4);
}

/* Call each widget type's dispose function.
 * 0x136100 / objects.obj
 */
void FUN_00136100(void)
{
  short sVar1;
  void **ppuVar2;

  sVar1 = 0;
  ppuVar2 = (void **)0x32353c;
  do {
    if ((sVar1 < 0) || (4 < sVar1)) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }
    if (*ppuVar2 != 0) {
      ((void (*)(void)) * ppuVar2)();
    }
    sVar1 = sVar1 + 1;
    ppuVar2 = ppuVar2 + 10;
  } while (sVar1 < 5);
}

void FUN_00136150(int object_handle)
{
  int *obj;
  char *tag_data;
  int *widget_block; /* tag block at tag+0x14c */
  int *element;
  int widget_handle;
  char *widget;
  int definition_handle;
  int16_t i;
  int16_t type;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  tag_data = (char *)tag_get(0x6f626a65, obj[0]);

  widget_block = (int *)(tag_data + 0x14c);

  /* Initialize widget list head to NONE. */
  *(int *)((char *)obj + 0x11c) = -1;

  if (*widget_block <= 0)
    return;

  for (i = 0; (int)i < *widget_block; i++) {
    element = (int *)tag_block_get_element(widget_block, (int)i, 0x20);

    /* Search the widget_types table for a matching group_tag. */
    for (type = 0; type < 5; type++) {
      if (*(int *)(0x323528 + (int)type * 0x28) == element[0])
        break;
    }
    if (type >= 5)
      continue;

    /* Found a match. Skip if type is NONE or definition index is NONE. */
    if (type == -1)
      continue;
    if (element[3] == -1)
      continue;

    /* Assert: type is in valid range [0, NUMBER_OF_WIDGET_TYPES). */
    if (type < 0 || type >= 5) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }

    /* Allocate a new widget datum. */
    widget_handle = data_new_at_index(*(data_t **)0x5a90c4);
    if (widget_handle == -1)
      continue;

    widget = (char *)datum_get(*(data_t **)0x5a90c4, widget_handle);

    /* Store the widget type. */
    *(int16_t *)(widget + 0x2) = type;

    /* Check if this widget type has a "new" function (entry+0x18). */
    if (*(int (**)(int))(0x323528 + (int)type * 0x28 + 0x18) == 0) {
      /* No new function — link directly with definition = NONE. */
      *(int *)(widget + 0x8) = *(int *)((char *)obj + 0x11c);
      *(int *)((char *)obj + 0x11c) = widget_handle;
      *(int *)(widget + 0x4) = -1;
    } else {
      /* Call the widget type's new function with the definition index. */
      definition_handle =
        (*(int (**)(int))(0x323528 + (int)type * 0x28 + 0x18))(element[3]);
      *(int *)(widget + 0x4) = definition_handle;
      if (definition_handle == -1) {
        /* New function failed — delete the widget datum. */
        datum_delete(*(data_t **)0x5a90c4, widget_handle);
      } else {
        /* Success — link into the object's widget list. */
        *(int *)(widget + 0x8) = *(int *)((char *)obj + 0x11c);
        *(int *)((char *)obj + 0x11c) = widget_handle;
      }
    }
  }
}

/* FUN_001362d0 — delete all widgets from an object's widget list.
 * Walks the linked list of widgets at obj+0x11c, calling each widget type's
 * delete_proc (at widget_types[type]+0x1c) if the widget has a valid
 * definition handle, then deletes the widget datum from the pool.
 *
 * Widget structure (0xc bytes):
 *   +0x02: type (int16_t) - index into widget_types table
 *   +0x04: definition_handle (int) - handle returned by new_proc, or -1
 *   +0x08: next_widget_handle (int) - linked list next pointer
 *
 * Widget types table at 0x323528 (5 entries, 0x28 bytes each):
 *   +0x00: group_tag
 *   +0x18: new_proc
 *   +0x1c: delete_proc
 *
 * Source: c:\halo\SOURCE\objects\widgets\widgets.c (line 0xbe)
 * Assert: c:\halo\source\objects\widgets\widget_types.h (line 0x96)
 */
void FUN_001362d0(int object_handle)
{
  int *obj;
  int widget_handle;
  char *widget;
  int next_handle;
  int16_t type;

  obj = (int *)object_get_and_verify_type(object_handle, -1);

  widget_handle = *(int *)((char *)obj + 0x11c);
  if (widget_handle == -1) {
    *(int *)((char *)obj + 0x11c) = -1;
    return;
  }

  do {
    widget = (char *)datum_get(*(data_t **)0x5a90c4, widget_handle);
    type = *(int16_t *)(widget + 0x2);

    /* Assert: type is in valid range [0, NUMBER_OF_WIDGET_TYPES). */
    if (type < 0 || type >= 5) {
      display_assert("type>=0 && type<NUMBER_OF_WIDGET_TYPES",
                     "c:\\halo\\source\\objects\\widgets\\widget_types.h", 0x96,
                     1);
      system_exit(-1);
    }

    next_handle = *(int *)(widget + 0x8);

    /* If this widget has a valid definition handle, call delete_proc. */
    if (*(int *)(widget + 0x4) != -1) {
      if (*(int (**)(int))(0x323528 + (int)type * 0x28 + 0x1c) == 0) {
        display_assert("type_definition->delete_proc",
                       "c:\\halo\\SOURCE\\objects\\widgets\\widgets.c", 0xbe,
                       1);
        system_exit(-1);
      }
      (*(void (**)(int))(0x323528 + (int)type * 0x28 + 0x1c))(
        *(int *)(widget + 0x4));
    }

    datum_delete(*(data_t **)0x5a90c4, widget_handle);
    widget_handle = next_handle;
  } while (next_handle != -1);

  *(int *)((char *)obj + 0x11c) = -1;
}

/*
 * object_wake — disconnect a point light from the cluster partition.
 * (from c:\halo\SOURCE\objects\object_lights.c, line 0x4d0)
 *
 * Looks up the light datum in the point-light data table at 0x5a90bc.
 * If the light is active (flags bit 1) and connected to the map (flags bit 2),
 * removes it from the cluster partition at 0x5a90b0, then clears the
 * connected_to_map flag.
 *
 * Confirmed: datum_get(*(data_t**)0x5a90bc, object_handle) — 2 cdecl args.
 * Confirmed: TEST AL,0x2 for active flag, TEST AL,0x4 for connected_to_map.
 * Confirmed: cluster_partition_remove_object(0x5a90b0, handle, light+0x10).
 * Confirmed: AND byte ptr [ESI+0x2],0xfb clears bit 2.
 */
void object_wake(int object_handle)
{
  char *light;
  uint16_t flags;

  light = (char *)datum_get(*(data_t **)0x5a90bc, object_handle);
  flags = *(uint16_t *)(light + 0x2);

  if ((flags & 0x2) == 0)
    return;

  if ((flags & 0x4) == 0) {
    display_assert("TEST_FLAG(light->flags, _point_light_connected_to_map_bit)",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x4d0, 1);
    system_exit(-1);
  }

  cluster_partition_remove_object((void *)0x5a90b0, object_handle,
                                  (void *)(light + 0x10));
  *(uint8_t *)(light + 0x2) &= ~0x4;
}

/* Call cluster_partition_iter_first on the object cluster partition at
 * 0x5a90b0. 0x1398b0 / objects.obj
 */
void FUN_001398b0(int *param_1, int param_2)
{
  cluster_partition_iter_first((void *)0x5a90b0, param_1, (int16_t)param_2);
}

/* Call cluster_partition_iter_next on the object cluster partition at 0x5a90b0.
 * 0x1398d0 / objects.obj
 */
void FUN_001398d0(int *param_1)
{
  cluster_partition_iter_next((void *)0x5a90b0, param_1);
}

/* Check if the lights marker global has changed; update it and return 1 if so.
 * 0x139990 / objects.obj
 */
int FUN_00139990(int param_1)
{
  int iVar1;

  iVar1 = (int)datum_get(*(data_t **)0x5a90bc, param_1);
  if (*(char *)0x5a8d60 == '\0') {
    display_assert("lights_globals.marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x67f, 1);
    system_exit(-1);
  }
  if (*(int *)(iVar1 + 0xc) != *(int *)0x5a8d64) {
    *(int *)(iVar1 + 0xc) = *(int *)0x5a8d64;
    return 1;
  }
  return 0;
}

/*
 * object_move_to_limbo — compute a point light's world-space position,
 * direction, and range from its parent object, then add it to the cluster
 * partition so it becomes visible.
 * (from c:\halo\SOURCE\objects\object_lights.c, line ~0x4f9)
 *
 * Two paths for computing position/orientation:
 *   1. No parent (parent_handle == -1): use marker definition from the object
 *      tag to fill position/forward/up on the light.
 *   2. Has parent: transform the light's offset point and direction through
 *      the parent's node matrix.
 *
 * Then computes the effective light range from the tag's radius, color
 * modifier, and optional gel modifier. The range determines a position offset
 * based on the falloff angle:
 *   - angle >= pi/2: position unchanged, range = computed range
 *   - pi/4 <= angle < pi/2: position offset by range * forward_modifier along
 *     forward, range scaled by tertiary modifier
 *   - angle < pi/4: position offset by range / forward_modifier along forward
 *
 * Finally adds the light to the cluster partition and sets the connected flag.
 *
 * Confirmed: SUB ESP,0x84 — 132 bytes of locals.
 * Confirmed: tag_get(0x6c696768, ...) for 'ligh' tag.
 * Confirmed: cluster_partition_add_object(0x5a90b0, ...) with 6 args.
 * Confirmed: OR word ptr [ESI+0x2], BX sets connected_to_map (bit 2).
 */
void object_move_to_limbo(int object_handle)
{
  char *light;
  char *parent_obj;
  char *marker_def;
  void *node_matrix;
  char marker_buf[0x6c]; /* output from object_get_markers_by_string_id */
  char location[8]; /* scenario location (cluster_index etc.) */
  float local_pos[3]; /* computed light position */
  float local_range; /* computed effective range */
  uint8_t tag_flags;
  float falloff_angle;
  float offset;
  int16_t marker_index;

  light = (char *)datum_get(*(data_t **)0x5a90bc, object_handle);
  tag_get(0x6c696768, *(int *)(light + 0x4));

  if (*(int *)(light + 0x58) == -1) {
    /* No parent object index: compute from marker definition in tag. */
    marker_index = *(int16_t *)(light + 0x5c);
    marker_def = (char *)object_get_child_marker_definition(
      *(int *)(light + 0x2c), marker_index);
    object_get_markers_by_string_id(*(int *)(light + 0x2c), marker_def,
                                    marker_buf, 1);

    /* Copy position from marker buffer offset 0x60 → light+0x30 */
    *(float *)(light + 0x30) = *(float *)(marker_buf + 0x60);
    *(float *)(light + 0x34) = *(float *)(marker_buf + 0x64);
    *(float *)(light + 0x38) = *(float *)(marker_buf + 0x68);
    /* Copy forward from marker buffer offset 0x3c → light+0x3c */
    *(float *)(light + 0x3c) = *(float *)(marker_buf + 0x3c);
    *(float *)(light + 0x40) = *(float *)(marker_buf + 0x40);
    *(float *)(light + 0x44) = *(float *)(marker_buf + 0x44);
    /* Copy up from marker buffer offset 0x54 → light+0x48 */
    *(float *)(light + 0x48) = *(float *)(marker_buf + 0x54);
    *(float *)(light + 0x4c) = *(float *)(marker_buf + 0x58);
    *(float *)(light + 0x50) = *(float *)(marker_buf + 0x5c);
  } else {
    /* Has parent: transform offset through parent's node matrix. */
    parent_obj =
      (char *)object_try_and_get_and_verify_type(*(int *)(light + 0x2c), -1);
    if (parent_obj != 0) {
      marker_index = *(int16_t *)(light + 0x5c);
      node_matrix =
        object_get_node_matrix(*(int *)(light + 0x2c), marker_index);
      matrix_transform_point((float *)node_matrix, (float *)(light + 0x60),
                             (float *)(light + 0x30));
      matrix_transform_vector((float *)node_matrix, (float *)(light + 0x6c),
                              (float *)(light + 0x3c));
      perpendicular3d((float *)(light + 0x3c), (float *)(light + 0x48));
      normalize3d((float *)(light + 0x48));
    }
  }

  if ((*(uint16_t *)(light + 0x2) & 0x2) == 0)
    return;

  /* Re-fetch light data (original code re-calls datum_get here). */
  {
    char *light2 = (char *)datum_get(*(data_t **)0x5a90bc, object_handle);
    char *ligh_tag = (char *)tag_get(0x6c696768, *(int *)(light2 + 0x4));

    tag_flags = *(uint8_t *)ligh_tag;
    local_range = *(float *)(ligh_tag + 0xc) * *(float *)(ligh_tag + 0x4);

    if ((tag_flags & 0x2) == 0)
      local_range *= *(float *)(ligh_tag + 0x24);

    if (local_range < *(float *)(ligh_tag + 0x18)) {
      /* Range below cutoff: use position directly, clamp to cutoff. */
      local_pos[0] = *(float *)(light2 + 0x30);
      local_pos[1] = *(float *)(light2 + 0x34);
      local_pos[2] = *(float *)(light2 + 0x38);
      local_range = *(float *)(ligh_tag + 0x18);
    } else {
      falloff_angle = *(float *)(ligh_tag + 0x14);
      if (falloff_angle >= *(float *)0x2568bc) {
        /* angle >= pi/2: use position as-is. */
        local_pos[0] = *(float *)(light2 + 0x30);
        local_pos[1] = *(float *)(light2 + 0x34);
        local_pos[2] = *(float *)(light2 + 0x38);
      } else if (falloff_angle >= *(float *)0x254a58) {
        /* pi/4 <= angle < pi/2: offset along forward, scale range. */
        offset = local_range * *(float *)(ligh_tag + 0x20);
        local_pos[0] =
          offset * *(float *)(light2 + 0x3c) + *(float *)(light2 + 0x30);
        local_pos[1] =
          offset * *(float *)(light2 + 0x40) + *(float *)(light2 + 0x34);
        local_pos[2] =
          offset * *(float *)(light2 + 0x44) + *(float *)(light2 + 0x38);
        local_range = local_range * *(float *)(ligh_tag + 0x28);
      } else {
        /* angle < pi/4: offset = range / forward_modifier. */
        local_range = local_range / *(float *)(ligh_tag + 0x20);
        local_pos[0] =
          local_range * *(float *)(light2 + 0x3c) + *(float *)(light2 + 0x30);
        local_pos[1] =
          local_range * *(float *)(light2 + 0x40) + *(float *)(light2 + 0x34);
        local_pos[2] =
          local_range * *(float *)(light2 + 0x44) + *(float *)(light2 + 0x38);
      }
    }

    /* Assert: light must NOT already be connected to map. */
    if ((*(uint8_t *)(light + 0x2) & 0x4) != 0) {
      display_assert(
        "!TEST_FLAG(light->flags, _point_light_connected_to_map_bit)",
        "c:\\halo\\SOURCE\\objects\\object_lights.c", 0x4f9, 1);
      system_exit(-1);
    }

    /* Determine cluster location for the light. */
    if (*(int *)(light + 0x2c) == -1 ||
        object_try_and_get_and_verify_type(*(int *)(light + 0x2c), -1) == 0) {
      scenario_location_from_point((void *)location, (void *)local_pos);
    } else {
      object_get_location(*(int *)(light + 0x2c), (void *)location);
    }

    /* Add light to cluster partition. Range is passed as raw float bits
     * reinterpreted as uint32_t (radius_fp convention). */
    {
      union {
        float f;
        uint32_t u;
      } range_bits;
      range_bits.f = local_range;
      cluster_partition_add_object((void *)0x5a90b0, object_handle,
                                   (void *)(light + 0x10), (void *)local_pos,
                                   range_bits.u, (void *)location);
    }

    *(uint16_t *)(light + 0x2) |= 0x4;
  }
}

/* Create a new point light datum from a light tag (0x13b290).
 * Allocates from the light data table (0x5a90bc), validates the 'ligh' tag,
 * initializes fields, then calls object_move_to_limbo to resolve world-space
 * position. If object_handle is -1 (world light), position/forward are stored
 * directly; otherwise marker index and local offsets are stored for attachment.
 */
int FUN_0013b290(int tag_index, int object_handle, int16_t marker,
                 float *position, float *forward, int unknown)
{
  int handle;
  char *datum;

  handle = data_new_at_index(*(data_t **)0x5a90bc);
  if (handle != -1) {
    datum = (char *)datum_get(*(data_t **)0x5a90bc, handle);
    tag_get(0x6c696768, tag_index);
    *(int16_t *)(datum + 0x02) = 0;
    {
      int tick = game_time_get();
      *(uint8_t *)(datum + 0x02) |= 3;
      *(int *)(datum + 0x58) = tick;
    }
    *(int *)(datum + 0x04) = tag_index;
    *(int *)(datum + 0x2c) = object_handle;
    *(int *)(datum + 0x78) = unknown;
    *(int *)(datum + 0x10) = -1;

    if (object_handle == -1) {
      *(float *)(datum + 0x30) = position[0];
      *(float *)(datum + 0x34) = position[1];
      *(float *)(datum + 0x38) = position[2];
      *(float *)(datum + 0x3c) = forward[0];
      *(float *)(datum + 0x40) = forward[1];
      *(float *)(datum + 0x44) = forward[2];
    } else {
      *(int16_t *)(datum + 0x5c) = marker;
      *(float *)(datum + 0x60) = position[0];
      *(float *)(datum + 0x64) = position[1];
      *(float *)(datum + 0x68) = position[2];
      *(float *)(datum + 0x6c) = forward[0];
      *(float *)(datum + 0x70) = forward[1];
      *(float *)(datum + 0x74) = forward[2];
    }

    object_move_to_limbo(handle);
    *(int *)(datum + 0x0c) = *(int *)0x5a8d64 - 1;
  }
  return handle;
}

/* FUN_0013bce0 — Compute object lighting from BSP lightmap/environment.
 * Samples lighting at the object's position and 4 offset positions, averages
 * successful samples. The original left local_88 uninitialized; when all 5
 * lookups fail (BSP transition), stack residue from clang-compiled ported
 * functions contains x87 NaN that permanently poisons the render state shadow
 * color. Fixed by zero-initializing the fallback buffer.
 * (0x13bce0 / objects.obj, object_lights.c:0x3ca) */
void FUN_0013bce0(int object_handle, float *lighting)
{
  int *obj;
  int tag_data;
  uint32_t flags;
  float local_88[29];
  float offset_pos[3];
  int16_t sample_count;
  uint16_t corner;
  char ok;
  float scale;
  int i;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  flags = 0;

  if (lighting == NULL) {
    display_assert("lighting", "c:\\halo\\SOURCE\\objects\\object_lights.c",
                   0x3ca, 1);
    system_exit(-1);
  }

  if ((uint8_t)(obj[1] >> 8) & 0x80)
    flags = 1;

  tag_data = (int)tag_get(0x6f626a65, *obj);
  if (*(uint8_t *)(tag_data + 2) & 4)
    flags |= 4;

  csmemset(local_88, 0, sizeof(local_88));

  ok =
    ((char (*)(uint32_t, int *, float *))0x13ab20)(flags, obj + 0x14, lighting);

  if ((obj[1] & 0x4000) != 0)
    return;

  if (ok == '\0') {
    sample_count = 0;
    csmemset(lighting, 0, 0x74);
    *(uint16_t *)(lighting + 3) = 2;
  } else {
    sample_count = 1;
  }

  for (corner = 0; (int16_t)corner < 4; corner++) {
    float xoff;
    float yoff;

    xoff = *(float *)0x29b5e0;
    if (corner & 1)
      xoff = *(float *)0x254b50;
    yoff = *(float *)0x29b5e0;
    if (corner & 2)
      yoff = *(float *)0x254b50;

    offset_pos[0] = xoff * *(float *)(obj + 0x17) + *(float *)(obj + 0x14);
    offset_pos[1] = yoff * *(float *)(obj + 0x17) + *(float *)(obj + 0x15);
    *(int *)(offset_pos + 2) = obj[0x16];

    ok = ((char (*)(uint32_t, float *, float *))0x13ab20)(flags, offset_pos,
                                                          local_88);
    if (ok != '\0') {
      sample_count++;
      for (i = 0; i < 3; i++)
        lighting[i] += local_88[i];
      lighting[0x13] += local_88[0x13];
      lighting[0x14] += local_88[0x14];
      lighting[0x15] += local_88[0x15];
      lighting[0x16] += local_88[0x16];
      for (i = 4; i <= 0xf; i++)
        lighting[i] += local_88[i];
      lighting[0x1a] += local_88[0x1a];
      lighting[0x1b] += local_88[0x1b];
      lighting[0x1c] += local_88[0x1c];
      lighting[0x17] += local_88[0x17];
      lighting[0x18] += local_88[0x18];
      lighting[0x19] += local_88[0x19];
    }
  }

  if (sample_count > 1) {
    scale = *(float *)0x2533c8 / (float)(int)sample_count;
    for (i = 0; i < 3; i++)
      lighting[i] *= scale;
    lighting[0x13] *= scale;
    lighting[0x14] *= scale;
    lighting[0x15] *= scale;
    lighting[0x16] *= scale;
    for (i = 4; i <= 6; i++)
      lighting[i] *= scale;
    normalize3d(lighting + 7);
    for (i = 10; i <= 12; i++)
      lighting[i] *= scale;
    normalize3d(lighting + 0xd);
    lighting[0x1a] *= scale;
    lighting[0x1b] *= scale;
    lighting[0x1c] *= scale;
    lighting[0x17] *= scale;
    lighting[0x18] *= scale;
    lighting[0x19] *= scale;
    normalize3d(lighting + 0x17);
  } else if (sample_count == 0) {
    for (i = 0; i < 29; i++)
      lighting[i] = local_88[i];
  }
}

/* 0x13c100 / objects.obj */
void *FUN_0013c100(int16_t object_type)
{
  int iVar1;

  if ((object_type < 0) || (0xb < object_type)) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d isn't a valid object type in [#0,#%d)",
                            (int)object_type, 0xc),
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x277, 1);
    system_exit(-1);
  }
  iVar1 = (int)object_type;
  if (((void **)0x324608)[iVar1] == (void *)0) {
    display_assert("object_type_definitions[object_type]",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x278, 1);
    system_exit(-1);
  }
  if (*(int *)((char *)((void **)0x324608)[iVar1] + 4) == 0) {
    display_assert("object_type_definitions[object_type]->group_tag",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x279, 1);
    system_exit(-1);
  }
  return ((void **)0x324608)[iVar1];
}

int FUN_0013c490(int object_handle);

/* 0x13c250 / objects.obj */
void *FUN_0013c250(int16_t param_1)
{
  int iVar1;

  if (param_1 < 0 || 0xb < param_1) {
    display_assert(csprintf((char *)0x5ab100,
                            "#%d isn't a valid object type in [#0,#%d)",
                            (int)param_1, 0xc),
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x28c, 1);
    system_exit(-1);
  }
  iVar1 = (int)param_1;
  if (((void **)0x324608)[iVar1] == (void *)0) {
    display_assert("object_type_definitions[object_type]",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x28d, 1);
    system_exit(-1);
  }
  return *(void **)((void **)0x324608)[iVar1];
}

/* Walk the object type definition list and call dispose at +0x14 on each.
 * 0x13c3a0 / objects.obj
 */
void FUN_0013c3a0(void)
{
  int iVar1;

  for (iVar1 = *(int *)0x5a8d54; iVar1 != 0; iVar1 = *(int *)(iVar1 + 0x9c)) {
    if (*(void (**)(void))(iVar1 + 0x14) != 0) {
      (*(void (**)(void))(iVar1 + 0x14))();
    }
  }
}

/* Reset slot counter, walk the list and call initialize_for_new_map at +0x18.
 * 0x13c3d0 / objects.obj
 */
void FUN_0013c3d0(void)
{
  int iVar1;

  *(int *)0x46f078 = 0;
  for (iVar1 = *(int *)0x5a8d54; iVar1 != 0; iVar1 = *(int *)(iVar1 + 0x9c)) {
    if (*(void (**)(void))(iVar1 + 0x18) != 0) {
      (*(void (**)(void))(iVar1 + 0x18))();
    }
  }
}

/* Walk the object type definition list and call dispose_from_old_map at +0x1c.
 * 0x13c400 / objects.obj
 */
void FUN_0013c400(void)
{
  int iVar1;

  for (iVar1 = *(int *)0x5a8d54; iVar1 != 0; iVar1 = *(int *)(iVar1 + 0x9c)) {
    if (*(void (**)(void))(iVar1 + 0x1c) != 0) {
      (*(void (**)(void))(iVar1 + 0x1c))();
    }
  }
}

/* Dispatch object placement callback at vtable +0x20 for all type extensions.
 * 0x13c430 / objects.obj
 */
void FUN_0013c430(int param_1, void *param_2)
{
  int *piVar1;
  int iVar2;
  int iVar3;
  short sVar4;

  iVar2 = (int)object_get_and_verify_type(param_1, 0xffffffff);
  iVar3 = (int)FUN_0013c100((int16_t) * (short *)(iVar2 + 100));
  piVar1 = (int *)(iVar3 + 0x5c);
  sVar4 = 0;
  iVar2 = *(int *)(iVar3 + 0x5c);
  while (iVar2 != 0) {
    if (*(void (**)(int, void *))(*piVar1 + 0x20) != 0) {
      (*(void (**)(int, void *))(*piVar1 + 0x20))(param_1, param_2);
    }
    sVar4 = sVar4 + 1;
    piVar1 = (int *)(iVar3 + 0x5c + (int)sVar4 * 4);
    iVar2 = *(int *)(iVar3 + 0x5c + (int)sVar4 * 4);
  }
}

/* Dispatch object type extension callback at vtable +0x28 for all extensions.
 * 0x13c500 / objects.obj
 */
void FUN_0013c500(int param_1, int param_2)
{
  int *piVar1;
  int iVar2;
  int iVar3;
  short sVar4;

  iVar2 = (int)object_get_and_verify_type(param_1, 0xffffffff);
  iVar3 = (int)FUN_0013c100((int16_t) * (short *)(iVar2 + 100));
  piVar1 = (int *)(iVar3 + 0x5c);
  sVar4 = 0;
  iVar2 = *(int *)(iVar3 + 0x5c);
  while (iVar2 != 0) {
    if (*(void (**)(int, int))(*piVar1 + 0x28) != 0) {
      (*(void (**)(int, int))(*piVar1 + 0x28))(param_1, param_2);
    }
    sVar4 = sVar4 + 1;
    piVar1 = (int *)(iVar3 + 0x5c + (int)sVar4 * 4);
    iVar2 = *(int *)(iVar3 + 0x5c + (int)sVar4 * 4);
  }
}

/* Dispatch object type extension callback at vtable +0x2c for all extensions.
 * 0x13c560 / objects.obj
 */
void FUN_0013c560(int param_1)
{
  int *piVar1;
  int iVar2;
  int iVar3;
  short sVar4;

  iVar2 = (int)object_get_and_verify_type(param_1, 0xffffffff);
  iVar3 = (int)FUN_0013c100((int16_t) * (short *)(iVar2 + 100));
  piVar1 = (int *)(iVar3 + 0x5c);
  sVar4 = 0;
  iVar2 = *(int *)(iVar3 + 0x5c);
  while (iVar2 != 0) {
    if (*(void (**)(int))(*piVar1 + 0x2c) != 0) {
      (*(void (**)(int))(*piVar1 + 0x2c))(param_1);
    }
    sVar4 = sVar4 + 1;
    piVar1 = (int *)(iVar3 + 0x5c + (int)sVar4 * 4);
    iVar2 = *(int *)(iVar3 + 0x5c + (int)sVar4 * 4);
  }
}

/* Dispatch object type extension callback at vtable +0x34 for all extensions.
 * 0x13c620 / objects.obj
 */
void FUN_0013c620(int param_1)
{
  int *piVar1;
  int iVar2;
  int iVar3;
  short sVar4;

  iVar2 = (int)object_get_and_verify_type(param_1, 0xffffffff);
  iVar3 = (int)FUN_0013c100((int16_t) * (short *)(iVar2 + 100));
  piVar1 = (int *)(iVar3 + 0x5c);
  sVar4 = 0;
  iVar2 = *(int *)(iVar3 + 0x5c);
  while (iVar2 != 0) {
    if (*(void (**)(int))(*piVar1 + 0x34) != 0) {
      (*(void (**)(int))(*piVar1 + 0x34))(param_1);
    }
    sVar4 = sVar4 + 1;
    piVar1 = (int *)(iVar3 + 0x5c + (int)sVar4 * 4);
    iVar2 = *(int *)(iVar3 + 0x5c + (int)sVar4 * 4);
  }
}

/* Dispatch vtable slot +0x38 for each extension in the object type's table.
 * 0x13c680 / objects.obj
 */
void FUN_0013c680(int param_1, int param_2)
{
  int *piVar1;
  int iVar2;
  int iVar3;
  short sVar4;

  iVar2 = (int)object_get_and_verify_type(param_1, 0xffffffff);
  iVar3 = (int)FUN_0013c100((int16_t) * (short *)(iVar2 + 100));
  piVar1 = (int *)(iVar3 + 0x5c);
  sVar4 = 0;
  iVar2 = *(int *)(iVar3 + 0x5c);
  while (iVar2 != 0) {
    if (*(void (**)(int, int))(*piVar1 + 0x38) != 0) {
      (*(void (**)(int, int))(*piVar1 + 0x38))(param_1, param_2);
    }
    sVar4 = sVar4 + 1;
    piVar1 = (int *)(iVar3 + 0x5c + (int)sVar4 * 4);
    iVar2 = *(int *)(iVar3 + 0x5c + (int)sVar4 * 4);
  }
}

/*
 * FUN_0013c6e0 — dispatch a region-destroyed callback through the object
 * type definition's extension table.
 *
 * Resolves the object's type, looks up its type definition via FUN_0013c100,
 * then walks the pointer array at type_def+0x5c. For each non-NULL entry,
 * reads a function pointer at entry+0x3c and calls it with the original
 * three arguments (object_handle, param_2, param_3).
 *
 * Called from damage.c (FUN_00137690) when a region is destroyed, passing
 * (object_handle, region_index, region_flags).
 *
 * Confirmed: cdecl, 3 args (ADD ESP,0xc at caller and inside loop).
 * Confirmed: MOVSX word [EAX+0x64] — reads object type as int16_t.
 * Confirmed: PUSH -1, PUSH EBX -> object_get_and_verify_type(handle, -1).
 * Confirmed: loop counter is int16_t (MOVSX EAX,SI at 0x13c728).
 * Confirmed: vtable offset 0x3c (MOV EAX,[EAX+0x3c] at 0x13c712).
 * Confirmed: indirect call passes all 3 params (PUSH ECX/EDX/EBX at
 * 0x13c71f-0x13c721).
 */
/* 0x13c6e0 */
void FUN_0013c6e0(int object_handle, int region_index, unsigned int flags)
{
  typedef void (*type_callback_t)(int, int, unsigned int);
  char *obj;
  char *type_def;
  char *entry;
  type_callback_t fn;
  int16_t i;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  type_def = (char *)FUN_0013c100(*(int16_t *)(obj + 0x64));

  i = 0;
  entry = *(char **)(type_def + 0x5c);
  while (entry != NULL) {
    fn = *(type_callback_t *)(entry + 0x3c);
    if (fn != NULL) {
      fn(object_handle, region_index, flags);
    }
    i = i + 1;
    entry = *(char **)(type_def + 0x5c + (int)(int16_t)i * 4);
  }
}

/*
 * FUN_0013c740 — walk the object type definition extension table and check
 * whether any extension's callback at offset +0x40 returns true.
 *
 * Resolves the object's type via object_get_and_verify_type(-1), looks up
 * the type definition via FUN_0013c100, then walks the NULL-terminated
 * pointer array at type_def+0x5c. For each non-NULL entry, reads a function
 * pointer at entry+0x40 and calls it with the object handle. If any callback
 * returns non-zero, the function returns 1 (sticky OR).
 *
 * Called from FUN_00136840, which recursively walks child objects. If this
 * function returns 0, the caller recurses into the child.
 *
 * Confirmed: cdecl, 1 arg (ADD ESP,0x4 after indirect CALL).
 * Confirmed: returns char/bool in AL (MOV AL,BL at 0x13c79a).
 * Confirmed: MOVSX EAX,SI — loop counter is int16_t.
 * Confirmed: vtable offset +0x40 (MOV EAX,[EAX+0x40] at 0x13c772).
 * Confirmed: XOR BL,BL — result initialized to 0, set to 1 on any true return.
 */
/* 0x13c740 */
char FUN_0013c740(int object_handle)
{
  typedef char (*type_check_callback_t)(int);
  char *obj;
  char *type_def;
  char *entry;
  type_check_callback_t fn;
  char result;
  int16_t i;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  type_def = (char *)FUN_0013c100(*(int16_t *)(obj + 0x64));

  result = 0;
  i = 0;
  entry = *(char **)(type_def + 0x5c);
  while (entry != NULL) {
    fn = *(type_check_callback_t *)(entry + 0x40);
    if (fn != NULL) {
      if (fn(object_handle) != 0) {
        result = 1;
      }
    }
    i = i + 1;
    entry = *(char **)(type_def + 0x5c + (int)(int16_t)i * 4);
  }
  return result;
}

/* Dispatch vtable slot +0x44 for each extension in the object type's table.
 * 0x13c7a0 / objects.obj
 */
void FUN_0013c7a0(int param_1, int param_2)
{
  int *piVar1;
  int iVar2;
  int iVar3;
  short sVar4;

  iVar2 = (int)object_get_and_verify_type(param_1, 0xffffffff);
  iVar3 = (int)FUN_0013c100((int16_t) * (short *)(iVar2 + 100));
  piVar1 = (int *)(iVar3 + 0x5c);
  sVar4 = 0;
  iVar2 = *(int *)(iVar3 + 0x5c);
  while (iVar2 != 0) {
    if (*(void (**)(int, int))(*piVar1 + 0x44) != 0) {
      (*(void (**)(int, int))(*piVar1 + 0x44))(param_1, param_2);
    }
    sVar4 = sVar4 + 1;
    piVar1 = (int *)(iVar3 + 0x5c + (int)sVar4 * 4);
    iVar2 = *(int *)(iVar3 + 0x5c + (int)sVar4 * 4);
  }
}

int object_header_block_allocate(int object_handle, int offset, int size);
/*
 * FUN_0013c800 — dispatch an animation-block initializer callback through the
 * object type definition's extension table.
 *
 * Resolves the object's type, looks up its type definition via FUN_0013c100,
 * then walks the NULL-terminated pointer array at type_def+0x5c. For each
 * non-NULL entry, reads a function pointer at entry+0x48 and calls it with
 * (object_handle, block_data).
 *
 * Called from FUN_0013e1a0 after resolving the animation block reference,
 * passing the object handle and the resolved block data pointer.
 *
 * Confirmed: cdecl, 2 args (ADD ESP,0x8 after indirect CALL).
 * Confirmed: MOVSX word [EAX+0x64] — reads object type as int16_t.
 * Confirmed: PUSH -1, PUSH EBX -> object_get_and_verify_type(handle, -1).
 * Confirmed: loop counter is int16_t (MOVSX EDX,SI at 0x13c844).
 * Confirmed: vtable offset 0x48 (MOV EAX,[EAX+0x48] at 0x13c832).
 * Confirmed: indirect call passes 2 params (PUSH ECX, PUSH EBX at
 * 0x13c83c-0x13c83d).
 */
/* 0x13c800 */
void FUN_0013c800(int object_handle, void *block_data)
{
  typedef void (*type_anim_callback_t)(int, void *);
  char *obj;
  char *type_def;
  char *entry;
  type_anim_callback_t fn;
  int16_t i;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  type_def = (char *)FUN_0013c100(*(int16_t *)(obj + 0x64));

  i = 0;
  entry = *(char **)(type_def + 0x5c);
  while (entry != NULL) {
    fn = *(type_anim_callback_t *)(entry + 0x48);
    if (fn != NULL) {
      fn(object_handle, block_data);
    }
    i = i + 1;
    entry = *(char **)(type_def + 0x5c + (int)(int16_t)i * 4);
  }
}

/*
 * FUN_0013c860 — dispatch per-type reset callbacks for an object.
 *
 * Looks up the object's type via FUN_0013c100(object_type), then iterates
 * the null-terminated handler array at type_data+0x5c. For each handler,
 * calls the reset function pointer at handler+0x4c with object_handle.
 * Used internally by object_reset to apply type-specific re-initialization.
 *
 * 0x13c860 / objects.obj
 */
void FUN_0013c860(int object_handle)
{
  char *obj;
  char *type_data;
  int *ptr;
  void (*reset_fn)(int);
  short cnt;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  type_data = (char *)FUN_0013c100((int16_t) * (short *)(obj + 0x64));
  ptr = (int *)(type_data + 0x5c);
  cnt = 0;

  while (*ptr != 0) {
    reset_fn = *(void (**)(int))((char *)*ptr + 0x4c);
    if (reset_fn != NULL)
      reset_fn(object_handle);
    cnt++;
    ptr = (int *)(type_data + 0x5c + (int)cnt * 4);
  }
}

/* Dispatch vtable slot +0x50 for each extension in the object type's table.
 * 0x13c8c0 / objects.obj
 */
void FUN_0013c8c0(int param_1)
{
  int *piVar1;
  int iVar2;
  int iVar3;
  short sVar4;

  iVar2 = (int)object_get_and_verify_type(param_1, 0xffffffff);
  iVar3 = (int)FUN_0013c100((int16_t) * (short *)(iVar2 + 100));
  piVar1 = (int *)(iVar3 + 0x5c);
  sVar4 = 0;
  iVar2 = *(int *)(iVar3 + 0x5c);
  while (iVar2 != 0) {
    if (*(void (**)(int))(*piVar1 + 0x50) != 0) {
      (*(void (**)(int))(*piVar1 + 0x50))(param_1);
    }
    sVar4 = sVar4 + 1;
    piVar1 = (int *)(iVar3 + 0x5c + (int)sVar4 * 4);
    iVar2 = *piVar1;
  }
}

/* Dispatch vtable slot +0x58 for each extension in the object type's table.
 * 0x13c920 / objects.obj
 */
void FUN_0013c920(int param_1)
{
  int *piVar1;
  int iVar2;
  int iVar3;
  short sVar4;

  iVar2 = (int)object_get_and_verify_type(param_1, 0xffffffff);
  iVar3 = (int)FUN_0013c100((int16_t) * (short *)(iVar2 + 100));
  piVar1 = (int *)(iVar3 + 0x5c);
  sVar4 = 0;
  iVar2 = *(int *)(iVar3 + 0x5c);
  while (iVar2 != 0) {
    if (*(void (**)(int))(*piVar1 + 0x58) != 0) {
      (*(void (**)(int))(*piVar1 + 0x58))(param_1);
    }
    sVar4 = sVar4 + 1;
    piVar1 = (int *)(iVar3 + 0x5c + (int)sVar4 * 4);
    iVar2 = *piVar1;
  }
}

/* Dispatch vtable slot +0x54 for each extension in the object type's table.
 * 0x13c980 / objects.obj
 */
void FUN_0013c980(int param_1, int param_2, int param_3)
{
  int *piVar1;
  int iVar2;
  int iVar3;
  short sVar4;

  iVar2 = (int)object_get_and_verify_type(param_1, 0xffffffff);
  iVar3 = (int)FUN_0013c100((int16_t) * (short *)(iVar2 + 100));
  piVar1 = (int *)(iVar3 + 0x5c);
  sVar4 = 0;
  iVar2 = *(int *)(iVar3 + 0x5c);
  while (iVar2 != 0) {
    if (*(void (**)(int, int, int))(*piVar1 + 0x54) != 0) {
      (*(void (**)(int, int, int))(*piVar1 + 0x54))(param_1, param_2, param_3);
    }
    sVar4 = sVar4 + 1;
    piVar1 = (int *)(iVar3 + 0x5c + (int)sVar4 * 4);
    iVar2 = *(int *)(iVar3 + 0x5c + (int)sVar4 * 4);
  }
}

/* Return a pointer into the scenario's placement block for an object type.
 * 0x13ca30 / objects.obj
 */
int FUN_0013ca30(int param_1, int param_2, int *param_3)
{
  int iVar1;

  iVar1 = (int)FUN_0013c100((short)param_2);
  if (*(short *)(iVar1 + 10) == -1) {
    display_assert("definition->placement_tag_block_offset!=NONE",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x4ff, 1);
    system_exit(-1);
  }
  if (((short)*(unsigned short *)(iVar1 + 10) < 0) ||
      (0x5bc < *(unsigned short *)(iVar1 + 10))) {
    display_assert(
      "definition->placement_tag_block_offset>=0 && "
      "definition->placement_tag_block_offset<=sizeof(struct scenario)"
      "+sizeof(struct tag_block)",
      "c:\\halo\\SOURCE\\objects\\object_types.c", 0x500, 1);
    system_exit(-1);
  }
  if (param_3 != (int *)0x0) {
    *param_3 = (int)*(short *)(iVar1 + 0xe);
  }
  return *(short *)(iVar1 + 10) + param_1;
}

/* Return a pointer into the scenario's palette block for an object type.
 * 0x13cab0 / objects.obj
 */
int FUN_0013cab0(int param_1, int param_2)
{
  int iVar1;

  iVar1 = (int)FUN_0013c100((short)param_2);
  if (*(short *)(iVar1 + 0xc) == -1) {
    display_assert("definition->palette_tag_block_offset!=NONE",
                   "c:\\halo\\SOURCE\\objects\\object_types.c", 0x50d, 1);
    system_exit(-1);
  }
  if (((short)*(unsigned short *)(iVar1 + 0xc) < 0) ||
      (0x5bc < *(unsigned short *)(iVar1 + 0xc))) {
    display_assert(
      "definition->palette_tag_block_offset>=0 && "
      "definition->palette_tag_block_offset<=sizeof(struct scenario)"
      "+sizeof(struct tag_block)",
      "c:\\halo\\SOURCE\\objects\\object_types.c", 0x50e, 1);
    system_exit(-1);
  }
  return *(short *)(iVar1 + 0xc) + param_1;
}

/* Wrap cluster_partition_iter_first for the non-collideable partition
 * (0x5a8d30). 0x13d570 / objects.obj
 */
void cluster_get_first_noncollideable_object(int *param_1, int param_2)
{
  cluster_partition_iter_first((void *)0x5a8d30, param_1, (int16_t)param_2);
}

/* Wrap cluster_partition_iter_next for the non-collideable partition
 * (0x5a8d30). 0x13d590 / objects.obj
 */
void cluster_get_next_noncollideable_object(int *param_1)
{
  cluster_partition_iter_next((void *)0x5a8d30, param_1);
}

/*
 * cluster_partition_object_iter_first (0x13d5b0) — begin iteration over
 * objects in a BSP cluster using the collideable partition (0x5a8d40).
 *
 * Wraps cluster_partition_iter_first with the collideable object partition
 * constant. Returns the first object handle in the cluster, or -1 if none.
 *
 * Confirmed: PUSH EAX (param_2=cluster_idx), PUSH ECX (param_1=state),
 *            PUSH 0x5a8d40, CALL 0x191a50. EAX passed through.
 * Confirmed: ADD ESP,0xc (3 cdecl args cleaned by caller).
 */
int cluster_partition_object_iter_first(int *state, int16_t cluster_idx)
{
  return cluster_partition_iter_first((void *)0x5a8d40, state, cluster_idx);
}

/*
 * cluster_partition_object_iter_next (0x13d5d0) — advance iteration over
 * objects in a BSP cluster using the collideable partition (0x5a8d40).
 *
 * Wraps cluster_partition_iter_next with the collideable object partition
 * constant. Returns the next object handle, or -1 when exhausted.
 *
 * Confirmed: PUSH EAX (param_1=state), PUSH 0x5a8d40, CALL 0x191660.
 * Confirmed: ADD ESP,0x8 (2 cdecl args cleaned by caller).
 */
int cluster_partition_object_iter_next(int *state)
{
  return cluster_partition_iter_next((void *)0x5a8d40, state);
}

/*
 * object_try_and_get_and_verify_type — resolve a datum handle to its
 * object_data_t*, returning NULL if the handle is invalid or the object's
 * type is not among the bits in type_mask.
 *
 * Uses datum_absolute_index_to_index (0x119270, a "try-and-get" that returns
 * 0/NULL on failure) instead of datum_get (which asserts).
 * Reads the compact type byte at header+0x03, not the int16 at object+0x64.
 *
 * Confirmed: CALL 0x119270 with 2 args (ADD ESP,0x8).
 * Confirmed: byte ptr [EDX+0x3] — reads header->type as uint8_t.
 * Confirmed: MOV EAX, [EDX+0x8] — returns header->object.
 * Confirmed: XOR EAX,EAX before both exit paths — returns NULL on failure.
 */
void *object_try_and_get_and_verify_type(int datum_handle, int type_mask)
{
  object_header_data_t *header =
    (object_header_data_t *)(int)datum_absolute_index_to_index(
      *(data_t **)0x5a8d50, datum_handle);
  if (header != NULL && (type_mask & (1 << (header->type & 0x1f))) != 0)
    return header->object;
  return NULL;
}

/*
 * object_get_and_verify_type — resolve a datum handle to its object_data_t*
 * and assert that the object's type is one of the bits in type_mask.
 *
 * The "object" data table pointer lives at 0x5a8d50 (allocated by
 * objects_initialize as the "object" header data array; distinct from
 * the "objects" memory pool at 0x46f080 and object_header_data at 0x5abc10).
 *
 * datum_get(data, handle) returns object_header_data_t*; field at +8 is the
 * object_data_t* . Type enum is a signed int16 at object_data_t+0x64.
 *
 * Confirmed: MOVSX ECX, word ptr [ESI+0x64] — signed 16-bit read.
 * Confirmed: ADD ESP,0x8 after datum_get (2 cdecl args).
 * Confirmed: ADD ESP,0x10 after csprintf (4 args cleaned; 3 pre-pushed remain
 *            on stack for display_assert); ADD ESP,0x14 cleans the rest.
 */
void *object_get_and_verify_type(int datum_handle, int type_mask)
{
  /* datum_get: first arg = data table ptr (value at 0x5a8d50) */
  object_header_data_t *header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, datum_handle);
  object_data_t *obj = header->object;
  int16_t type = obj->type;

  if ((type_mask & (1 << (type & 0x1f))) == 0) {
    /* csprintf with varargs: buffer, format, type_mask, (int)type.
     * The remaining 3 args (filename, lineno, halt) are pre-pushed before
     * csprintf in the original; in C we pass them explicitly to display_assert.
     * Confirmed: ADD ESP,0x10 cleans 4 csprintf args; display_assert receives
     * (reason, filepath, lineno, halt). */
    char *msg = csprintf((char *)0x5ab100,
                         "got an object type we didn't expect (expected one of "
                         "0x%08x but got #%d).",
                         type_mask, (int)type);
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
    system_exit(-1);
  }
  return obj;
}

/*
 * object_iterator_new (0x13d6f0) — initialise an object_iter_t for a walk.
 *
 * Calls data_verify on the object data table (sanity check), then writes
 * the caller-supplied type_mask and flags into the iterator block and
 * resets its scan state.
 *
 * Layout of object_iter_t (confirmed from disassembly):
 *   +0x00 int32_t  type_mask     — accepted object types (1<<type bit-mask)
 *   +0x04 uint8_t  flags         — required header flag byte (AND+CMP filter)
 *   +0x06 int16_t  current_index — next header-table slot to probe
 *   +0x08 int32_t  last_handle   — handle from last call (NONE = -1 on init)
 *   +0x0c uint32_t cookie        — 0x86868686 (marks as initialized)
 *
 * Confirmed: ADD ESP,0x4 after data_verify (1 arg).
 * Confirmed: byte ptr [EAX+0x4] = DL (flags, byte-sized arg).
 * Confirmed: word ptr [EAX+0x6] = 0x0000; dword ptr [EAX+0x8] = -1.
 * Confirmed: dword ptr [EAX+0xc] = 0x86868686 (cookie, written last).
 */
void object_iterator_new(void *iter, int type_mask, int flags)
{
  object_iter_t *it = (object_iter_t *)iter;
  data_verify(*(data_t **)0x5a8d50);
  it->cookie = 0x86868686;
  it->type_mask = type_mask;
  it->flags = (uint8_t)flags;
  it->current_index = 0;
  it->last_handle = NONE;
}

/*
 * object_iterator_next (0x13d730) — advance iterator, return next match.
 *
 * Walks the object header table starting at iter->current_index, scanning
 * for a non-empty slot (salt != 0) whose header flags satisfy the required
 * flag mask (entry_flags & iter->flags == iter->flags) and whose type bit
 * is set in iter->type_mask.  On a match:
 *   - Stores the composite handle (salt<<16 | index) in iter->last_handle.
 *   - Advances iter->current_index past the matched slot.
 *   - Returns the object_data_t* from entry->object (header+0x8).
 *
 * Returns NULL when the table is exhausted.
 *
 * The header table is an array of 0xc-byte object_header_data_t entries;
 * pointers start at data_t->data (offset +0x34 from the data_t header).
 * The live slot count is at data_t->current_count (offset +0x2e, int16_t).
 *
 * Confirmed: cookie guard == 0x86868686 (assert "uninitialized iterator").
 * Confirmed: MOVSX EAX, word ptr [EAX+0x2e] — current_count as signed 16-bit.
 * Confirmed: MOVSX from DX (current_index) into ECX for OR with shifted salt.
 * Confirmed: entry stride = 0xc (LEA ESI,[ESI+ECX*4] with ECX=index*3).
 * Confirmed: return entry->object at entry+0x8.
 */
void *object_iterator_next(void *iter)
{
  object_iter_t *it = (object_iter_t *)iter;
  data_t *data;
  object_header_data_t *entry;
  int16_t count;
  int16_t idx;

  if (it->cookie != 0x86868686) {
    display_assert("uninitialized iterator passed to object_iterator_next()",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x6b8, 1);
    system_exit(-1);
  }

  data_verify(*(data_t **)0x5a8d50);
  data = *(data_t **)0x5a8d50;

  idx = it->current_index;
  count = data->current_count;
  entry = (object_header_data_t *)((char *)data->data + (int)idx * 0xc);

  while (idx < count) {
    int handle = ((int)(uint16_t)entry->unk_0 << 16) | (int)(uint16_t)idx;
    idx++;
    if (entry->unk_0 != 0 && (entry->unk_2 & it->flags) == it->flags &&
        (it->type_mask & (1 << (entry->type & 0x1f))) != 0) {
      it->last_handle = handle;
      it->current_index = idx;
      return entry->object;
    }
    entry = (object_header_data_t *)((char *)entry + 0xc);
    if (idx >= count) {
      it->current_index = idx;
      return NULL;
    }
  }
  it->current_index = idx;
  return NULL;
}

/*
 * object_set_garbage_flag — add or remove an object from the garbage
 * collection linked list.
 *
 * The garbage list is a singly-linked list threaded through
 * object_data_t+0xC0 (unk_192), with the head stored at
 * object_globals+0x08 (unk_8).
 *
 * When is_garbage is nonzero (add to garbage list):
 *   - Bails out if bit 0x10000 (garbage) or 0x20000 is already set.
 *   - Prepends the object to the garbage list head.
 *   - Sets bit 0x10000 in object flags.
 *
 * When is_garbage is zero (remove from garbage list):
 *   - Bails out if bit 0x10000 is NOT set.
 *   - Walks the list to find and unlink the object.
 *   - Clears bit 0x10000 in object flags.
 *   - Sets unk_192 to NONE (-1).
 *
 * Two debug validation loops walk the entire garbage list before and
 * after the mutation, asserting that every entry has a valid type and
 * the garbage bit set. These correspond to lines 0x7a0 and 0x7d6 in
 * the original objects.c.
 *
 * Confirmed: 2 cdecl args — PUSH [EBP+8], PUSH -1 before CALL 0x13d680.
 * Confirmed: MOV AL, byte ptr [EBP+0xC] — second arg is char-sized.
 * Confirmed: TEST EAX,0x30000 guards the add path; TEST EAX,0x10000
 *            guards the remove path.
 * Confirmed: garbage list next at object+0xC0, head at og+0x08.
 * Confirmed: assert strings at 0x29b9c4 and line numbers 0x7a0, 0x7d6.
 * Confirmed: object_get_and_verify_type(handle, -1) to resolve.
 */

/*
 * object_get_root_parent — walk the parent chain to the root object.
 *
 * Starting from object_handle, loops through parent_object_index (obj+0xCC)
 * until it reaches -1.  Each iteration validates the object type against the
 * full-type mask (0xFFFFFFFF).  Returns the topmost non-null handle, or -1
 * if the input was already -1.
 *
 * Confirmed: datum_get(DAT_005a8d50, handle) -> header at +0x08 -> type at
 *            +0x64 (int16_t).  Bit-shift check (1 << (type & 0x1f)) against
 *            0 — in practice always passes since mask is -1.
 * Confirmed: Loop terminates when obj->parent_object_index == -1.
 */
int object_get_root_parent(int object_handle)
{
  int current;
  int result;

  if (object_handle == -1)
    return -1;

  current = object_handle;
  result = -1;
  while (current != -1) {
    object_header_data_t *header =
      (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, current);
    object_data_t *obj = header->object;
    result = current;
    current = obj->parent_object_index.value;
  }
  return result;
}

void FUN_0013d870(void)
{
}

/*
 * object_name_list_set_handle — store an object handle at a name-table index.
 *
 * Validates that param_1 is non-negative (TEST AX,AX / JL) and less than the
 * scenario's object-name count (at scenario+0x204), then writes param_2 into
 * the object_name_list array (pointer at 0x46f07c) at the given index.
 *
 * Confirmed: MOVSX ESI,AX — sign-extends param_1 before use.
 * Confirmed: MOV ECX,[0x46f07c] — dereferences pointer, not direct array.
 * Confirmed: MOV [ECX + ESI*4],EAX — stores param_2 at name_table[param_1].
 * Confirmed: cdecl, caller at 0x45ffb does ADD ESP,0x8 after call.
 */
void object_name_list_set_handle(short param_1, int param_2)
{
  int iVar1;

  if (param_1 < 0)
    return;
  iVar1 = (int)global_scenario_get();
  if (param_1 < *(int *)(iVar1 + 0x204)) {
    (*(int **)0x46f07c)[param_1] = param_2;
  }
}

void object_set_garbage_flag(int object_handle, int is_garbage)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  object_globals_t *og = object_globals;

  /* Pre-validation: walk the garbage list and assert integrity */
  {
    int handle = og->unk_8.value;
    while (handle != -1) {
      object_header_data_t *hdr =
        (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
      object_data_t *gobj = hdr->object;
      int16_t type = gobj->type;
      if ((1 << (type & 0x1f)) == 0) {
        char *msg = csprintf(
          (char *)0x5ab100,
          "got an object type we didn't expect (expected one of 0x%08x but "
          "got #%d).",
          -1, (int)type);
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
      if ((gobj->flags & 0x10000) == 0) {
        display_assert(
          "TEST_FLAG(garbage_object->object.flags, _object_garbage_bit)",
          "c:\\halo\\SOURCE\\objects\\objects.c", 0x7a0, 1);
        system_exit(-1);
      }
      handle = gobj->unk_192;
    }
    og = object_globals;
  }

  if ((char)is_garbage != 0) {
    /* Add to garbage list */
    if ((obj->flags & 0x30000) != 0)
      goto done;

    obj->unk_192 = og->unk_8.value;
    og->unk_8.value = object_handle;
    obj->flags |= 0x10000;
  } else {
    /* Remove from garbage list */
    uint32_t *prev_ptr;
    int cur;

    if ((obj->flags & 0x10000) == 0)
      goto done;

    /* Walk the list to find the previous pointer */
    prev_ptr = (uint32_t *)&og->unk_8.value;
    cur = og->unk_8.value;

    while (cur != object_handle) {
      object_header_data_t *hdr =
        (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, *prev_ptr);
      object_data_t *gobj = hdr->object;
      int16_t type = gobj->type;
      if ((1 << (type & 0x1f)) == 0) {
        char *msg = csprintf(
          (char *)0x5ab100,
          "got an object type we didn't expect (expected one of 0x%08x but "
          "got #%d).",
          -1, (int)type);
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
      prev_ptr = &gobj->unk_192;
      cur = gobj->unk_192;
    }

    /* Unlink: *prev_ptr = obj->next; obj->next = NONE */
    *prev_ptr = obj->unk_192;
    obj->unk_192 = 0xffffffff;
    obj->flags &= ~(uint32_t)0x10000;
  }

done:
  /* Post-validation: walk the garbage list again */
  {
    int handle = og->unk_8.value;
    while (handle != -1) {
      object_header_data_t *hdr =
        (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
      object_data_t *gobj = hdr->object;
      int16_t type = gobj->type;
      if ((1 << (type & 0x1f)) == 0) {
        char *msg = csprintf(
          (char *)0x5ab100,
          "got an object type we didn't expect (expected one of 0x%08x but "
          "got #%d).",
          -1, (int)type);
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
      if ((gobj->flags & 0x10000) == 0) {
        display_assert(
          "TEST_FLAG(garbage_object->object.flags, _object_garbage_bit)",
          "c:\\halo\\SOURCE\\objects\\objects.c", 0x7d6, 1);
        system_exit(-1);
      }
      handle = gobj->unk_192;
    }
  }
}

void garbage_collect_now(void)
{
  *(unsigned char *)(*(int *)0x46f084 + 2) = 1;
}

void FUN_0013dbe0(int param_1)
{
  int iVar1;

  iVar1 = *(int *)0x46f084;
  if (param_1 == -1) {
    *(short *)(*(int *)0x46f084 + 0x90) = 0;
    return;
  }
  *(short *)(iVar1 + 0x90) = 1;
  *(int *)(iVar1 + 0x94) = param_1;
}

void FUN_0013dcb0(void)
{
  *(short *)(*(int *)0x46f084 + 0x90) = 0;
}

void object_definition_predict(int param_1)
{
  void *tag;

  if (param_1 != -1) {
    tag = tag_get(0x6f626a65, param_1);
    predicted_resources_precache((char *)tag + 0x170);
  }
}

void object_beautify(int param_1, char param_2)
{
  int iVar1;

  if (param_1 != -1) {
    if (param_2 != '\0') {
      iVar1 = (int)object_get_and_verify_type(param_1, 0xffffffff);
      *(unsigned int *)(iVar1 + 4) = *(unsigned int *)(iVar1 + 4) | 0x400000;
      return;
    }
    iVar1 = (int)object_get_and_verify_type(param_1, 0xffffffff);
    *(unsigned int *)(iVar1 + 4) = *(unsigned int *)(iVar1 + 4) & 0xffbfffff;
  }
}

/*
 * object_header_new — allocate a new datum in an object data table and reserve
 * pool memory for it from the global objects memory pool at 0x46f080.
 *
 * If type_hint == -1, allocates at the next free index (data_new_at_index).
 * Otherwise allocates at the specified handle (data_new_datum).
 * On success, allocates datum_size bytes from the pool into datum+8,
 * records the size at datum+6, and zeros the allocated block.
 * Returns the datum handle, or -1 on failure.
 *
 * Confirmed: CMP EAX,-1 branches to data_new_at_index vs data_new_datum.
 * Confirmed: CALL 0x11e6c0 (memory_pool_block_new) with pool from [0x46f080].
 * Confirmed: MOV [EDI+6],CX stores datum_size as int16_t.
 * Confirmed: CALL 0x8db80 (csmemset) zeros *(void**)(datum+8).
 * Confirmed: datum_delete on pool allocation failure, returns -1.
 */
int object_header_new(data_t *data, int16_t datum_size, int type_hint)
{
  int handle;

  if (type_hint == -1)
    handle = data_new_at_index(data);
  else
    handle = data_new_datum(data, type_hint);

  if (handle != -1) {
    char *datum = (char *)datum_get(data, handle);

    if (!memory_pool_block_new(*(void **)0x46f080, (void **)(datum + 8),
                               (int)datum_size)) {
      datum_delete(data, handle);
      return -1;
    }

    *(int16_t *)(datum + 6) = datum_size;
    csmemset(*(void **)(datum + 8), 0, (int)datum_size);
  }

  return handle;
}

void object_postprocess_node_matrices(data_t *data);

/*
 * object_header_block_reference_get — resolve an object's inline
 *
 * block-reference pair ({size, offset}) to a pointer into object data.
 *
 *
 * Confirmed: CALL 0x119320 (datum_get) first, then CALL 0x13d680
 *
 * (object_get_and_verify_type).
 * Confirmed: reference fields are signed
 * 16-bit reads at +0 (size)
 * and +2 (offset).
 * Confirmed: asserts
 * "reference->offset>0" at line 0x98b and
 *
 * "reference->offset+reference->size<=object_header->data_size"
 * at line
 * 0x98c, both followed by system_exit(-1).
 * Confirmed: return value is
 * object_ptr + reference->offset.
 */
void *object_header_block_reference_get(int object_handle, void *reference)
{
  object_header_data_t *header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  char *object = (char *)object_get_and_verify_type(object_handle, -1);
  int16_t ref_size = *(int16_t *)reference;
  int16_t ref_offset = *(int16_t *)((char *)reference + 2);

  if (ref_offset < 1) {
    display_assert("reference->offset>0",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x98b, 1);
    system_exit(-1);
  }

  if ((int)header->data_size < (int)ref_size + (int)ref_offset) {
    display_assert(
      "reference->offset+reference->size<=object_header->data_size",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x98c, 1);
    system_exit(-1);
  }

  return object + ref_offset;
}

/*
 * FUN_0013e1a0 — run animation-block initializer callbacks for an object.
 *
 * Resolves the object's tag definition and checks whether both a model
 * (tag+0x34) and an animation graph (tag+0x44) are present. If so,
 * resolves the object's animation block reference at object_data+0x1a0
 * via object_header_block_reference_get, then dispatches through type
 * callbacks via FUN_0013c800.
 *
 * Confirmed: single register arg object_handle in EDI.
 * Confirmed: PUSH -1, PUSH EDI -> object_get_and_verify_type(handle, -1).
 * Confirmed: PUSH EAX, PUSH 0x6f626a65 -> tag_get('obje', obj[0]).
 * Confirmed: ADD ESP,0x10 cleans both calls (4 pushes).
 * Confirmed: CMP [EAX+0x34],-1 checks model tag index.
 * Confirmed: CMP [EAX+0x44],-1 checks animation graph tag index.
 * Confirmed: ADD ESI,0x1a0 -> object_data+0x1a0 is the animation block ref.
 * Confirmed: PUSH ESI, PUSH EDI -> object_header_block_reference_get(handle,
 * obj+0x1a0). Confirmed: PUSH EAX (return value), PUSH EDI ->
 * FUN_0013c800(handle, block). Confirmed: ADD ESP,0x10 cleans both calls (4
 * pushes).
 */
/* 0x13e1a0 */
void FUN_0013e1a0(int object_handle /* @<edi> */)
{
  char *obj;
  char *tag_data;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  tag_data = (char *)tag_get(0x6f626a65, *(int *)obj);

  if (*(int *)(tag_data + 0x34) != -1 && *(int *)(tag_data + 0x44) != -1) {
    void *block = object_header_block_reference_get(object_handle, obj + 0x1a0);
    FUN_0013c800(object_handle, block);
  }
}

/* Remove object_handle from a sibling linked list rooted at list_head.
 * Walks the chain at offset 0xc4 (next_sibling) until it finds the entry
 * matching object_handle, then unlinks it.
 * list_head in EAX, object_handle in EBX (register args). */
void object_child_list_remove(void *list_head /* @<eax> */,
                              int object_handle /* @<ebx> */)
{
  int *head = (int *)list_head;
  int *obj_data;

  if (*head == -1)
    return;

  while (1) {
    obj_data = (int *)datum_get(*(data_t **)0x5a8d50, *head);
    obj_data = (int *)*(int *)((char *)obj_data + 8);

    {
      int type = (int)*(int16_t *)((char *)obj_data + 0x64);
      if ((1 << (type & 0x1f)) == 0) {
        char *msg =
          csprintf((char *)0x5ab100,
                   "got an object type we didn't expect (expected one of "
                   "0x%08x but got #%d).",
                   -1, type);
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
    }

    if (*head == object_handle) {
      *head = *(int *)((char *)obj_data + 0xc4);
      *(int *)((char *)obj_data + 0xc4) = -1;
      return;
    }

    head = (int *)((char *)obj_data + 0xc4);
    if (*head == -1) {
      display_assert("*first_object_reference!=NONE",
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0xc6b, 1);
      system_exit(-1);
      if (*head == -1)
        return;
    }
  }
}

void object_scripting_set_collideable(int param_1, char param_2)
{
  int iVar1;

  if (param_1 != -1) {
    iVar1 = (int)object_get_and_verify_type(param_1, 0xffffffff);
    if (param_2 == '\0') {
      *(unsigned int *)(iVar1 + 4) = *(unsigned int *)(iVar1 + 4) | 0x1000000;
      return;
    }
    *(unsigned int *)(iVar1 + 4) = *(unsigned int *)(iVar1 + 4) & 0xfeffffff;
  }
}

/*
 * object_reset_markers — begin a marker sweep pass.
 *
 * Asserts that no marker pass is in progress, increments the global marker
 * generation counter (0x5a8d28), and sets
 * object_globals->object_marker_initialized to true.
 *
 * Confirmed: void, no params (no stack args referenced).
 * Confirmed: TEST byte ptr [EAX+1] — checks object_marker_initialized.
 * Confirmed: INC dword ptr [0x5a8d28] — increments generation counter.
 * Confirmed: MOV byte ptr [EAX+1], 1 — sets marker_initialized = true.
 */
void object_reset_markers(void)
{
  if (object_globals->object_marker_initialized) {
    display_assert("!object_globals->object_marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xdaf, 1);
    system_exit(-1);
  }
  *(uint32_t *)0x5a8d28 += 1;
  object_globals->object_marker_initialized = 1;
}

/*
 * object_marker_end (0x13ebc0) — end a marker sweep pass.
 *
 * Asserts that a marker pass is currently in progress
 * (object_marker_initialized must be true), then clears the flag to signal the
 * sweep is complete. Paired with object_reset_markers which begins the sweep.
 *
 * Confirmed: no prologue, no stack frame, no arguments.
 * Confirmed: MOV EAX,[0x46f084] -> object_globals.
 * Confirmed: MOV CL,[EAX+0x1] -> object_globals->object_marker_initialized.
 * Confirmed: TEST CL,CL; JNZ -> skips assert if initialized (true).
 * Confirmed: assert string "object_globals->object_marker_initialized" at line
 * 0xdba. Confirmed: CALL 0x8d9f0 (display_assert), CALL 0x8e2f0
 * (system_exit(-1)). Confirmed: ADD ESP,0x14 cleans 5 args (display_assert 4 +
 * system_exit 1). Confirmed: MOV byte ptr [EAX+0x1],0x0 -> clears
 * object_marker_initialized.
 */
void object_marker_end(void)
{
  if (!object_globals->object_marker_initialized) {
    display_assert("object_globals->object_marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xdba, 1);
    system_exit(-1);
  }
  object_globals->object_marker_initialized = 0;
}

/*
 * object_mark (0x13ec50) — mark an object with the current generation.
 *
 * Looks up the object (any type), asserts a marker sweep is in progress,
 * then compares the object's marker_generation (obj+0x08) against the
 * global generation counter at 0x5a8d28. If they differ, stamps the object
 * with the current generation and returns 1 (newly marked). If equal,
 * returns 0 (already marked this sweep).
 *
 * Confirmed: PUSH -1, PUSH EAX -> object_get_and_verify_type(handle, -1).
 * Confirmed: MOV ECX,[0x46f084]; MOV AL,[ECX+0x1] -> object_marker_initialized.
 * Confirmed: ADD ESP,0x8 cleans 2 args for object_get_and_verify_type.
 * Confirmed: assert "object_globals->object_marker_initialized" at line 0xdd7.
 * Confirmed: MOV EAX,[0x5a8d28] -> global marker generation counter.
 * Confirmed: CMP [ESI+0x8],EAX -> obj->marker_generation at offset 0x08.
 * Confirmed: MOV AL,0x1 / XOR AL,AL for return 1/0 (byte-sized).
 */
int object_mark(int object_handle)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  if (!object_globals->object_marker_initialized) {
    display_assert("object_globals->object_marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xdd7, 1);
    system_exit(-1);
  }

  if (obj->marker_generation != *(uint32_t *)0x5a8d28) {
    obj->marker_generation = *(uint32_t *)0x5a8d28;
    return 1;
  }
  return 0;
}

void attachments_new(int object_handle);

/* Propagate flags to all children of an object. For each child slot where
 * the "created" flag at obj+0xf4+i is clear and the child handle is valid,
 * optionally calls FUN_001396e0 (param_1) and/or FUN_0013aed0 (param_2).
 * object_handle in EAX (register arg). */
void object_propagate_flag_to_children(int object_handle /* @<eax> */,
                                       int param_1, int param_2)
{
  int *obj;
  void *tag_data;
  int16_t i;
  int count;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  if ((obj[1] & 0x100) == 0)
    return;

  tag_data = tag_get(0x6f626a65, obj[0]);
  count = *(int *)((char *)tag_data + 0x140);
  i = 0;
  while ((int)i < count) {
    if (*((char *)obj + 0xf4 + (int)i) == 0 && obj[(int)i + 0x3f] != -1) {
      if (param_1 != 0)
        object_wake(obj[(int)i + 0x3f]);
      if (param_2 != 0)
        object_move_to_limbo(obj[(int)i + 0x3f]);
    }
    i++;
  }
}

/* Remove an object from the scenario object-name lookup table.
 * Clears the name_index field (obj+0x6a) and removes all references
 * to object_handle from the name table at 0x46f07c.
 * object_handle in EDI (register arg). */
void object_remove_from_name_list(int object_handle /* @<edi> */)
{
  char *obj;
  void *scenario;
  int count;
  int *name_table;
  int16_t i;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if (*(int16_t *)(obj + 0x6a) == -1)
    return;

  scenario = global_scenario_get();
  *(int16_t *)(obj + 0x6a) = -1;
  count = *(int *)((char *)scenario + 0x204);
  name_table = *(int **)0x46f07c;
  i = 0;
  while ((int)i < count) {
    if (name_table[(int)i] == object_handle)
      name_table[(int)i] = -1;
    i++;
  }
}

/*
 * objects_place — place all scenario objects for the current map.
 *
 *
 * Sets the object_is_being_placed flag on object_globals, calls the scenario
 * object placer (FUN_0013cdd0, unported), then clears the flag. The flag is at
 * byte offset 0x00 of the object_globals struct.
 *
 * Confirmed: MOV byte ptr [EAX], 0x1 / MOV byte ptr [ECX], 0x0
 * Confirmed: global_scenario_get() result (EAX) pushed as sole arg to placer.
 * Confirmed: ADD ESP,0x4 after the placer call (1 cdecl arg).
 */
void objects_place(void)
{
  scenario_t *scenario;

  /* Set object_is_being_placed = true */
  object_globals->object_is_being_placed = 1;

  /* Get the scenario pointer and pass it to the object placer */
  scenario = global_scenario_get();
  ((pfn_int_t)0x13cdd0)((int)scenario);

  /* Clear object_is_being_placed */
  object_globals->object_is_being_placed = 0;
}

/* 0x13f080 - Walk the object tree recursively, collecting objects into an
 * output array. Starts from param_1, recurses depth-first into child (obj+0xc8)
 * then sibling (obj+0xc4). param_2: optional filter callback(handle, context) —
 * include object when non-zero; NULL = include all. param_3: opaque context
 * value forwarded to filter callback. param_4: current insertion index into
 * output array. param_5: maximum capacity of output array (stops when param_4
 * >= param_5). param_6: output array (int[]) that receives matching object
 * handles. Returns: updated count after processing this subtree. */
int FUN_0013f080(int param_1, char (*param_2)(int, int), int param_3,
                 int param_4, int param_5, int *param_6)
{
  void *local_c;

  local_c = object_get_and_verify_type(param_1, 0xffffffff);
  if (param_4 < param_5) {
    if (param_2 == 0 || (*param_2)(param_1, param_3) != '\0') {
      param_6[param_4] = param_1;
      param_4 = param_4 + 1;
    }
    if (*(int *)((char *)local_c + 0xc8) != -1) {
      param_4 = FUN_0013f080(*(int *)((char *)local_c + 0xc8), param_2, param_3,
                             param_4, param_5, param_6);
    }
    if (*(int *)((char *)local_c + 0xc4) != -1) {
      param_4 = FUN_0013f080(*(int *)((char *)local_c + 0xc4), param_2, param_3,
                             param_4, param_5, param_6);
    }
  }
  return param_4;
}

int sort_dumps(int param_1, int param_2)
{
  if (*(int *)(param_1 + 8) < *(int *)(param_2 + 8)) {
    return 1;
  }
  return (*(int *)(param_1 + 8) <= *(int *)(param_2 + 8)) - 1;
}

/*
 * objects_initialize — one-time initialisation of the object subsystem.
 *
 * Called once at startup (not per-map). Allocates the four root resources:
 *   - object header data table (data_t*) stored at 0x5a8d50
 *   - object memory pool (void*) stored at objects (0x46f080)
 *   - object_globals struct stored at object_globals (0x46f084)
 *   - object_name_list buffer stored at object_name_list (0x46f07c)
 * Then initialises the collideable and noncollideable cluster partition
 * structures at 0x5a8d40 and 0x5a8d30 via FUN_00191500.
 *
 * The allocation strategy differs between editor and non-editor modes:
 *   Non-editor: FUN_001bfe10("object", 0x800, 0xc) — data_array_new from
 *               game-state block; FUN_001bfe50("objects", 0x100000) —
 *               memory-pool_new from game-state block.
 *   Editor:     FUN_001194d0("object", 0x2800, 0xc) — data_array_new from
 *               main heap; memory_pool_new("objects", &DAT_500000) — memory-
 *               pool_new from main heap using a size read from 0x500000.
 *
 * Sub-system init call order (confirmed from disasm):
 *   FUN_00136580 — unknown object sub-type A init
 *   FUN_00135f90 — unknown object sub-type B init
 *   FUN_0013c2e0 — object type definition list init
 *   lights_initialize — object BSP cluster data init
 *
 * Confirmed: PUSH 0xc pre-pushed before JNZ — shared 3rd arg to both
 *            first-call variants; ADD ESP,0x14 cleans 5 args (3+2).
 * Confirmed: game_in_editor() returns bool via AL; TEST AL,AL / JNZ selects
 *            the editor allocation path.
 * Confirmed: ADD ESP,0x14 after display_assert+system_exit cleans 5 words
 *            (4 for display_assert + 1 for system_exit) in all 3 assert sites.
 * Confirmed: FUN_001bfbf0("object globals", 0, 0x98) allocates object_globals;
 *            FUN_001bfbf0("object name list", 0, 0x800) allocates name list.
 *            Both are game_state_alloc(name, tag?, size) — 3 cdecl args,
 *            ADD ESP,0xc each.
 * Confirmed: FUN_00191500 takes (void *partition, const char *name) — called
 *            twice; ADD ESP,0x10 cleans both calls (2 args * 2 calls).
 */
void objects_initialize(void)
{
  /* Initialise sub-systems (order confirmed from disasm) */
  ((pfn_void_t)0x136580)();
  ((pfn_void_t)0x135f90)();
  ((pfn_void_t)0x13c2e0)();
  ((pfn_void_t)0x1391e0)();

  if (!game_in_editor()) {
    /* Non-editor: allocate from game-state block */
    *(void **)0x5a8d50 =
      ((void *(*)(const char *, int, int))0x1bfe10)("object", 0x800, 0xc);
    objects = ((void *(*)(const char *, int))0x1bfe50)("objects", 0x100000);
  } else {
    /* Editor: allocate from main heap */
    *(void **)0x5a8d50 =
      ((void *(*)(const char *, int, int))0x1194d0)("object", 0x2800, 0xc);
    objects =
      ((void *(*)(const char *, void *))0x11e650)("objects", (void *)0x500000);
  }

  if (*(void **)0x5a8d50 == 0 || objects == 0) {
    display_assert("object_header_data && object_memory_pool",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xd8, 1);
    system_exit(-1);
  }

  object_globals = ((object_globals_t * (*)(const char *, int, int))0x1bfbf0)(
    "object globals", 0, 0x98);
  if (object_globals == 0) {
    display_assert("object_globals", "c:\\halo\\SOURCE\\objects\\objects.c",
                   0xdb, 1);
    system_exit(-1);
  }

  object_name_list =
    ((void *(*)(const char *, int, int))0x1bfbf0)("object name list", 0, 0x800);
  if (object_name_list == 0) {
    display_assert("object_name_list", "c:\\halo\\SOURCE\\objects\\objects.c",
                   0xfe8, 1);
    system_exit(-1);
  }

  /* Initialise collideable and noncollideable cluster partition structs */
  ((void (*)(void *, const char *))0x191500)((void *)0x5a8d40,
                                             "collideable object");
  ((void (*)(void *, const char *))0x191500)((void *)0x5a8d30,
                                             "noncollideable object");
}

/*
 * objects_initialize_for_new_map — reset object subsystems when loading a map.
 *
 * Call order (confirmed from disasm):
 *   FUN_001365a0  — resets a global slot index (object type slot reset)
 *   FUN_00136040  — iterates 5 object type slots, calls initialize_for_new_map
 *                   vtable entry via [EDI] (slot stride 0x28)
 *   FUN_0013c3d0  — walks the object_type_definition linked list, calls
 *                   each type's initialize_for_new_map function at +0x18
 *   lights_initialize_for_new_map  — calls data_delete_all on a BSP cluster
 * data table, then object_list_initialize_for_new_map via FUN_1915d0
 *
 * Then:
 *   data_delete_all(*(data_t**)0x5a8d50)        — clear all object headers
 *   csmemset(object_name_list, 0xff, 0x800)      — reset name list
 *   FUN_001915d0(&collideable_cluster_partition)  — reset collideable cluster
 *   FUN_001915d0(&noncollideable_cluster_partition) — reset noncollideable
 *   csmemset(object_globals->combined_pvs, 0, 64)
 *   csmemset(object_globals->combined_pvs_local, 0, 64)
 *   object_globals->pvs_activator_type = 0
 *   object_globals->object_marker_initialized = 0
 *   *(uint32_t*)0x5a8d28 = 0                     — unknown global counter
 *   object_globals->unk_8 = 0xffffffff            — datum handle sentinel
 *   object_globals->unk_4 = 0
 *   object_globals->last_garbage_collection_tick = 0
 *
 * Confirmed: ADD ESP,0x30 cleans 12 args across the 4 csmemset calls and
 *            the two FUN_1915d0 calls, consistent with 12 total cdecl pushes.
 */
void objects_initialize_for_new_map(void)
{
  object_globals_t *og;

  ((pfn_void_t)0x1365a0)();
  ((pfn_void_t)0x136040)();
  ((pfn_void_t)0x13c3d0)();
  ((pfn_void_t)0x1392b0)();

  data_delete_all(*(data_t **)0x5a8d50);
  csmemset(object_name_list, 0xff, 0x800);

  /* Reset collideable and noncollideable cluster partition structs.
   * These are 12-byte structs (3 data_t* fields) at fixed addresses. */
  ((void (*)(void *))0x1915d0)((void *)0x5a8d40);
  ((void (*)(void *))0x1915d0)((void *)0x5a8d30);

  og = object_globals;

  csmemset(og->combined_pvs, 0, 0x40);
  csmemset(og->combined_pvs_local, 0, 0x40);

  og->pvs_activator_type = 0;
  og->object_marker_initialized = 0;

  *(uint32_t *)0x5a8d28 = 0;

  og->unk_8.value = 0xffffffff;
  og->unk_4 = 0;
  og->last_garbage_collection_tick = 0;
}

/*
 * objects_dispose_from_old_map — per-map teardown of the object subsystem.
 *
 * Called when unloading a map (0x000a70a5 → this function). Counterpart to
 * objects_initialize_for_new_map (0x13f950). Distinct from objects_dispose
 * (0x13fac0), which is the one-time full teardown.
 *
 * Call order (confirmed from disasm):
 *   FUN_001365b0  — per-map dispose for type-slot array
 *   FUN_001360a0  — per-map dispose for 5 object-type slots
 *   FUN_0013c400  — per-map dispose for object type definition list
 *   lights_dispose_from_old_map  — per-map dispose for BSP cluster data
 *
 * Then, if the object header data table is valid (byte at data+0x24 != 0):
 *   Walk every datum via data_next_index (0x1198f0):
 *     - datum_get (0x119320) to retrieve element ptr (EBX)
 *     - if element->field_8 != NULL: memory_pool_free(objects,
 * &element->field_8)
 *     - datum_delete (0x1196d0) to remove the datum
 *     - zero element->field_8 and element->field_2
 *   After loop: data_make_invalid (0x119550) on the table
 *
 * Finally dispose collideable and noncollideable cluster partition structs:
 *   FUN_00191600(&collideable_cluster_partition)
 *   FUN_00191600(&noncollideable_cluster_partition)
 *
 * Confirmed: no arguments — caller at 0x000a70a5 uses bare CALL with no PUSH.
 *            Ghidra's __fastcall/param_1 is a misread of the PUSH ECX stack
 *            slot reservation in the function prologue.
 * Confirmed: MOV CL, byte ptr [EAX+0x24] / TEST CL,CL — byte guard on data
 *            valid flag (data_t.valid) before the loop.
 * Confirmed: ADD ESP,0x8 after datum_get and data_next_index calls (2 cdecl
 *            args each); ADD ESP,0x10 at loop-end cleans datum_delete (0x8) +
 *            data_next_index advance call (0x8) together.
 * Confirmed: ADD ESP,0x8 after memory_pool_free (2 cdecl args).
 * Confirmed: ADD ESP,0x4 after data_make_invalid (1 cdecl arg).
 * Confirmed: ADD ESP,0x8 cleans the two FUN_191600 calls at the end.
 * Confirmed: MOV dword ptr [EBP-4], EAX saves data ptr; reloaded at
 *            0x13fa5d for datum_delete after the conditional pool-free.
 * Confirmed: LEA EDI,[EBX+8] — EDI = &element->field_8 — passed as
 *            arg2 to memory_pool_free; also used to zero field_8 at 0x13fa67.
 */
void objects_dispose_from_old_map(void)
{
  data_t *obj_data;

  ((pfn_void_t)0x1365b0)();
  ((pfn_void_t)0x1360a0)();
  ((pfn_void_t)0x13c400)();
  ((pfn_void_t)0x1392e0)();

  obj_data = *(data_t **)0x5a8d50;

  /* Only walk the table if it has been made valid */
  if (*(uint8_t *)((uint8_t *)obj_data + 0x24) != 0) {
    int idx = data_next_index(obj_data, -1);
    while (idx != -1) {
      /* datum_get returns a pointer; field at +8 is the object data ptr */
      uint8_t *elem = (uint8_t *)datum_get(obj_data, idx);
      void **field_8_ptr = (void **)(elem + 0x8);

      if (*field_8_ptr != 0) {
        /* Free this object's memory pool allocation */
        ((void (*)(void *, void **))0x11e7a0)(*(void **)0x46f080, field_8_ptr);
      }

      datum_delete(obj_data, idx);

      /* Zero out field_8 and field_2 unconditionally after delete */
      *field_8_ptr = 0;
      *(uint8_t *)(elem + 0x2) = 0;

      idx = data_next_index(*(data_t **)0x5a8d50, idx);
    }
    data_make_invalid(*(data_t **)0x5a8d50);
  }

  /* Dispose cluster partition sub-tables */
  ((void (*)(void *))0x191600)((void *)0x5a8d40);
  ((void (*)(void *))0x191600)((void *)0x5a8d30);
}

/*
 * objects_dispose — tear down all object subsystems.
 *
 * Call order (confirmed from disasm):
 *   FUN_00136100  — iterates 5 type slots, calls dispose vtable entry at [EDI]
 *   FUN_0013c3a0  — walks linked list, calls each type's dispose at +0x14
 *   lights_dispose  — disposes the BSP cluster data, calls FUN_191630
 *
 * Then:
 *   if (!game_in_editor()):  null out *(data_t**)0x5a8d50 (don't free)
 *   else:                    data_dispose(*(data_t**)0x5a8d50)
 *
 *   if (objects != NULL):    objects = NULL  (pool not freed here)
 *
 *   FUN_00191630(&collideable_cluster_partition)   — zero 3 ptr fields
 *   FUN_00191630(&noncollideable_cluster_partition)
 *
 * Confirmed: JNZ selects data_dispose path; JZ / JNZ gates objects null.
 * Confirmed: ADD ESP,0x4 after data_dispose (1 cdecl arg).
 * Confirmed: ADD ESP,0x8 cleans 2 FUN_191630 calls at end.
 */
void objects_dispose(void)
{
  ((pfn_void_t)0x136100)();
  ((pfn_void_t)0x13c3a0)();
  ((pfn_void_t)0x1392a0)();

  if (!game_in_editor()) {
    /* Not in editor: just null the pointer, do not free */
    data_t **obj_data_ptr = (data_t **)0x5a8d50;
    if (*obj_data_ptr != 0) {
      *obj_data_ptr = 0;
    }
  } else {
    data_t *obj_data = *(data_t **)0x5a8d50;
    data_dispose(obj_data);
  }

  if (objects != 0) {
    objects = 0;
  }

  /* Zero out cluster partition structs (3 data_t* fields each) */
  ((void (*)(void *))0x191630)((void *)0x5a8d40);
  ((void (*)(void *))0x191630)((void *)0x5a8d30);
}

/*
 * object_activate — mark a root object as "outdoor"/visible if it passes
 * the activation conditions: not already active (bit 0x01), not flagged
 * 0x100000, and has no parent.
 *
 * Confirmed: CALL 0x119320 (datum_get) + CALL 0x13d680
 * (object_get_and_verify_type). Confirmed: tests header->unk_2 bit 0x01,
 * obj->flags bit 0x100000, obj->parent_object_index == -1. Confirmed: sets
 * header->unk_2 |= 0x01 on success.
 */
void object_activate(int object_handle)
{
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  if ((hdr->unk_2 & 0x01) == 0 && (obj->flags & 0x100000) == 0 &&
      obj->parent_object_index.value == -1) {
    hdr->unk_2 |= 0x01;
  }
}

/*
 * object_deactivate — clear the "active" flag (bit 0x01) from an object's
 * header unk_2 byte, if currently set.
 *
 * Inverse of object_activate: deactivates the object by clearing bit 0x01.
 *
 * Confirmed: CALL 0x119320 (datum_get), CALL 0x13d680
 *   (object_get_and_verify_type) with type_mask=-1.
 * Confirmed: TEST AL,0x1; JZ skip; AND AL,0xFE; MOV [ESI+2],AL.
 * Confirmed: ADD ESP,0x10 cleans datum_get + object_get_and_verify_type.
 */
void object_deactivate(int object_handle)
{
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  (void)obj; /* return value unused but call required for verification */
  if ((hdr->unk_2 & 0x01) != 0) {
    hdr->unk_2 &= ~0x01;
  }
}

/*
 * object_reset — reset an object to its default world position.
 *
 * Copies the 3-float default-position vector from *(float**)0x31fc38 into
 * both the object's position (+0x18..+0x20) and forward/up (+0x3c..+0x44),
 * clears the "at-rest" flag (bit 5 of object[+4]), then calls
 * FUN_0013c860 to perform a final physics/placement update.
 *
 * Confirmed: single call to object_get_and_verify_type(handle, -1), then
 * two identical 3-dword copies from [0x31fc38]. The 3 cdecl args
 * (−1, handle, handle) are batch-cleaned by ADD ESP,0xc after FUN_0013c860.
 *
 * 0x13fbc0 / objects.obj
 */
void object_reset(int object_handle)
{
  char *obj;
  float *def;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  def = *(float **)0x31fc38;
  *(float *)(obj + 0x18) = def[0];
  *(float *)(obj + 0x1c) = def[1];
  *(float *)(obj + 0x20) = def[2];
  *(float *)(obj + 0x3c) = def[0];
  *(float *)(obj + 0x40) = def[1];
  *(float *)(obj + 0x44) = def[2];
  *(unsigned int *)(obj + 4) &= ~0x20u;
  FUN_0013c860(object_handle);
}

/*
 * object_placement_data_new — initialise an object placement data struct.
 *
 * Zeroes the 0x88-byte placement buffer, stores the tag index at +0x00,
 * copies default forward {1,0,0} and up {0,0,1} vectors from constant
 * tables, then resolves the parent handle through the object header table:
 *   - If parent is valid: copies parent_object_index (+0x70), cluster
 *     index (+0x68), and the raw handle into the placement.
 *   - Otherwise: sets parent/cluster fields to -1/0xFFFF.
 * Finally fills four scale vectors at +0x58 with {1,1,1} each.
 *
 * Confirmed: csmemset(param_1, 0, 0x88) — 136-byte struct.
 * Confirmed: *(void**)0x31fc3c → {1.0, 0.0, 0.0} default forward.
 * Confirmed: *(void**)0x31fc44 → {0.0, 0.0, 1.0} default up.
 * Confirmed: *(void**)0x2ee708 → {1.0, 1.0, 1.0} default scale.
 * Confirmed: datum_absolute_index_to_index(DAT_005a8d50, parent_handle)
 *            returns header ptr; +0x3 = type, +0x8 = object ptr.
 * Confirmed: word at [ESI+0x16] = 0.
 * Confirmed: 4 iterations of 12-byte copy for scale at [ESI+0x58].
 */
void object_placement_data_new(void *placement, int tag_index,
                               int parent_handle)
{
  char *p = (char *)placement;
  float *src;
  int header;
  int obj;
  int i;

  csmemset(placement, 0, 0x88);

  /* +0x00: tag index */
  *(int *)(p + 0x00) = tag_index;
  /* +0x04: flags = 0 (already zeroed) */
  *(int *)(p + 0x04) = 0;

  /* +0x34: default forward vector {1,0,0} from *(void**)0x31fc3c */
  src = *(float **)0x31fc3c;
  *(float *)(p + 0x34) = src[0];
  *(float *)(p + 0x38) = src[1];
  *(float *)(p + 0x3c) = src[2];

  /* +0x40: default up vector {0,0,1} from *(void**)0x31fc44 */
  src = *(float **)0x31fc44;
  *(float *)(p + 0x40) = src[0];
  *(float *)(p + 0x44) = src[1];
  *(float *)(p + 0x48) = src[2];

  /* +0x16: zero (int16) */
  *(int16_t *)(p + 0x16) = 0;

  /* Resolve parent: datum_absolute_index_to_index returns header or 0 */
  header = datum_absolute_index_to_index(*(data_t **)0x5a8d50, parent_handle);
  if (header == 0 || (1 << (*(uint8_t *)(header + 0x3) & 0x1f)) == 0 ||
      *(int *)(header + 0x8) == 0) {
    /* No valid parent */
    *(int *)(p + 0x0c) = -1;
    *(int *)(p + 0x08) = -1;
    *(int16_t *)(p + 0x14) = -1;
  } else {
    obj = *(int *)(header + 0x8);
    *(int *)(p + 0x0c) = parent_handle;
    *(int *)(p + 0x08) = *(int *)(obj + 0x70);
    *(int16_t *)(p + 0x14) = *(int16_t *)(obj + 0x68);
  }

  /* +0x58: four {1,1,1} scale vectors from *(void**)0x2ee708 */
  {
    char *dst = p + 0x58;
    for (i = 4; i != 0; i--) {
      src = *(float **)0x2ee708;
      *(float *)(dst + 0x0) = src[0];
      *(float *)(dst + 0x4) = src[1];
      *(float *)(dst + 0x8) = src[2];
      dst += 0xc;
    }
  }
}

/*
 * object_disconnect_from_map — remove an object from the BSP cluster
 * partition and its parent's child chain, then clear the
 * _object_connected_to_map_bit (0x800) flag.
 *
 * If the object has a parent (parent_object_index != NONE), it unlinks
 * itself from the parent's child-object linked list starting at
 * parent_obj+0xC8 (via the list-remove helper at 0x13e510, which walks
 * next_object_index links at obj+0xC4).
 *
 * If the object has no parent, it removes itself from the appropriate
 * cluster partition (0x5a8d40 for objects with flag 0x2000000 set,
 * 0x5a8d30 otherwise) via the partition-remove call at 0x1919a0. It
 * then optionally clears the "outdoor" bit (header byte+2, bit 0x1)
 * if the header's bit 0x40 flag is set.
 *
 * Confirmed: single cdecl arg (object_handle).
 * Confirmed: assert strings reference objects.c lines 0x3bd and 0x3be.
 * Confirmed: 0x13e510 reads EAX (ptr to first_child_ref) and EBX
 *   (object_handle) as register args with 0 stack args.
 * Confirmed: 0x1919a0 is cdecl with 3 stack args (partition, handle, ptr).
 */
void object_disconnect_from_map(int object_handle)
{
  object_header_data_t *header;
  object_data_t *obj;

  header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  obj = header->object;

  /* assert: identifier portion of handle must be nonzero */
  assert_halt(object_handle & 0xffff0000);

  /* assert: object must be connected to map */
  assert_halt(obj->flags & 0x800);

  if (obj->parent_object_index.value != NONE) {
    /* Object has a parent: unlink from parent's child chain.
     * Get the parent's object data, then call the list-remove helper
     * at 0x13e510 with EAX = &parent_obj->unk_200 (child list head)
     * and EBX = object_handle to unlink. */
    object_data_t *parent_obj = (object_data_t *)object_get_and_verify_type(
      obj->parent_object_index.value, -1);
    object_child_list_remove((void *)((char *)parent_obj + 0xc8),
                             object_handle);
  } else {
    /* No parent: remove from cluster partition. */
    object_data_t *self_obj =
      (object_data_t *)object_get_and_verify_type(object_handle, -1);
    void *partition;
    if (self_obj->flags & 0x2000000)
      partition = (void *)0x5a8d40;
    else
      partition = (void *)0x5a8d30;

    cluster_partition_remove_object(partition, object_handle,
                                    (void *)((char *)obj + 0xbc));

    /* If header bit 0x40 is set, re-fetch header and clear bit 0x1 */
    if (header->unk_2 & 0x40) {
      object_header_data_t *header2 =
        (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
      object_get_and_verify_type(object_handle, -1);
      if (header2->unk_2 & 0x1) {
        header2->unk_2 &= ~0x1;
      }
    }
  }

  /* Clear _object_connected_to_map_bit (0x800) in object flags */
  obj->flags &= ~(uint32_t)0x800;

  /* Clear bit 0x20 in header flags byte */
  header->unk_2 &= ~0x20;
}

/* Get the node matrices reference block for an object.
 * Returns the header block reference at offset 0x1a0 from the object header.
 * 0x13fe70 / objects.obj
 */
void *object_get_node_matrices(int object_handle)
{
  void *obj = object_get_and_verify_type(object_handle, 0xffffffff);
  return object_header_block_reference_get(object_handle, (char *)obj + 0x1a0);
}

/*
 * object_get_child_marker_definition — get a marker definition from the
 * object's child model tag.
 *
 * Resolves the object's tag data via tag_get('obje', obj->tag_index),
 * then checks if marker_index is in range [0, block_count at tag+0x140).
 * If valid, returns tag_block_get_element(tag+0x140, marker_index, 0x48) +
 * 0x10. Returns NULL if index is out of range or negative.
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type), with -1 mask.
 * Confirmed: CALL 0x1ba140 (tag_get) with 'obje' (0x6f626a65) group tag.
 * Confirmed: CALL 0x19b210 (tag_block_get_element) with block at tag+0x140,
 *            element size 0x48, returns pointer + 0x10.
 * Confirmed: CMP CX,0 / CMP ECX,EDX — signed short check against block count.
 */
void *object_get_child_marker_definition(int object_handle,
                                         int16_t marker_index)
{
  uint32_t *obj = (uint32_t *)object_get_and_verify_type(object_handle, -1);
  int tag = (int)tag_get(0x6f626a65, (int)*obj);

  if (marker_index >= 0 && marker_index < *(int *)(tag + 0x140)) {
    return (char *)tag_block_get_element((void *)(tag + 0x140), marker_index,
                                         0x48) +
           0x10;
  }
  return NULL;
}

/*
 * object_has_node — check whether a given node index is valid for an object.
 *
 * Returns true if the object's model tag ('mode') has a nodes block and
 * node_index falls within [0, node_count). If the object tag definition has
 * no model reference (tag+0x34 == -1), returns true only when node_index == 0
 * (the implicit root node).
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type), 2 stack args.
 * Confirmed: CALL 0x1ba140 (tag_get) twice — first with 'obje', then 'mode'.
 * Confirmed: CMP word ptr [EBP+0xc],0x0 — node_index is int16_t.
 * Confirmed: model node count at offset 0xb8 in model tag data.
 * Confirmed: XOR BL,BL — false default; MOV AL,0x1 for true paths.
 */
bool object_has_node(int object_handle, int16_t node_index)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Look up the object's tag definition ('obje') */
  void *obje_tag = tag_get(0x6f626a65, (int)obj->tag_index);
  int model_tag_index = *(int *)((char *)obje_tag + 0x34);

  if (model_tag_index == -1) {
    /* No model — only node 0 (implicit root) is valid */
    if (node_index == 0)
      return true;
  } else {
    /* Look up the model tag ('mode') and check node count at offset 0xb8 */
    void *mode_tag = tag_get(0x6d6f6465, model_tag_index);
    if (node_index >= 0 && (int)node_index < *(int *)((char *)mode_tag + 0xb8))
      return true;
  }

  return false;
}

/*
 * object_set_automatic_deactivation — set or clear the "hidden" flag (bit 0x40)
 * on an object's header unk_2 byte, and optionally activate or deactivate the
 * object.
 *
 * When param_2 != 0 (hide):
 *   Sets bit 0x40 on hdr->unk_2. If the object has no parent
 *   (parent_object_index == -1) AND unk_76.index == -1, calls
 *   object_deactivate to deactivate (clear bit 0x01).
 *
 * When param_2 == 0 (unhide):
 *   Clears bit 0x40 from hdr->unk_2. If bit 0x01 is not set (i.e. the
 *   object is not currently active), calls object_activate.
 *
 * Confirmed: CALL 0x119320 (datum_get), CALL 0x13d680
 *   (object_get_and_verify_type) with type_mask=-1.
 * Confirmed: OR byte [ESI+2],0x40 in true branch; AND AL,0xBF in false.
 * Confirmed: CMP dword [EAX+0xCC],-1 (parent_object_index.value).
 * Confirmed: CMP word [EAX+0x4C],-1 (unk_76.index, 16-bit compare).
 * Confirmed: CALL 0x13fb80 (deactivate) and CALL 0x13fb30 (activate).
 * Confirmed: ADD ESP,0x10 cleans datum_get + object_get_and_verify_type.
 */
void object_set_automatic_deactivation(int object_handle, char param_2)
{
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  if (param_2 != 0) {
    hdr->unk_2 |= 0x40;
    if (obj->parent_object_index.value == -1 && obj->unk_76.index == -1) {
      object_deactivate(object_handle);
    }
  } else {
    uint8_t val = hdr->unk_2 & ~0x40;
    hdr->unk_2 = val;
    if ((val & 0x01) == 0) {
      object_activate(object_handle);
    }
  }
}

/*
 * object_set_garbage — set or clear the "garbage" activation state for
 * an object and its attached children.
 *
 * param flag: 0 = mark object as garbage (deactivate); non-zero = unmark.
 *
 * Reads the object's tag definition via tag_get to check whether the tag
 * has a children block (tag[0x34] != -1). If it does, and the object's
 * bit 0 of obj->flags (active/inactive state) is out of sync with the
 * requested flag, calls FUN_0013ee60 (via EAX register arg) to propagate
 * the state change to child objects before committing the datum update.
 *
 * FUN_0013ee60 (0x13ee60) takes (int object_handle @EAX, char param_1,
 *   char param_2) — 2 stack args, object_handle in EAX register. Uses
 *   args-array inline asm pattern to avoid EAX aliasing.
 *
 * Final datum update:
 *   flag==0: set obj->flags bit 0; clear datum byte[2] bit 1 (0x02).
 *   flag!=0: clear obj->flags bit 0; set datum byte[2] bit 1 (0x02).
 *
 * Confirmed: MOV EAX,EDI before CALL 0x13ee60 — EAX = object_handle.
 * Confirmed: ADD ESP,0x8 after CALL 0x13ee60 — 2 stack args.
 * Confirmed: ADD ESP,0x10 after tag_get (cleans 4 args: 2 for tag_get +
 *   2 pre-pushed for object_get_and_verify_type).
 * Confirmed: OR dword [ESI+4],1 — obj->flags |= 1 for flag==0 path.
 * Confirmed: AND byte [EAX+2],0xfd — hdr->unk_2 &= ~2 for flag==0 path.
 * Confirmed: AND dword [ESI+4],~1 — obj->flags &= ~1 for flag!=0 path.
 * Confirmed: OR byte [EAX+2],2 — hdr->unk_2 |= 2 for flag!=0 path.
 * Confirmed: DAT_005a8d50 as datum_get first arg.
 * Confirmed: CMP dword ptr [EBX+0x34],-1 — children block presence check.
 */
void object_set_garbage(int object_handle, int flag)
{
  /* ESI = object_data_t*, EDI = object_handle (saved for register-arg calls)
   */
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Get object tag definition; check if it has an attachments/children block
   * (non-null block at tag+0x34 == not -1). */
  void *tag_def = tag_get(0x6f626a65, (int)obj->tag_index);
  int has_children = (*(int *)((char *)tag_def + 0x34) != -1);
  int bit0 = (int)(obj->flags & 1);

  if (has_children) {
    if (bit0 != 0) {
      /* bit0 is set */
      if ((char)flag != 0) {
        /* Already inactive but being asked to unmark: propagate to children
         * with (param_1=0, param_2=1). */
        object_propagate_flag_to_children(object_handle, 0, 1);
        goto lab_0014000a;
      }
      /* bit0 set, flag==0: no child propagation needed */
      goto lab_0014000a;
    } else {
      /* bit0 is clear */
      if ((char)flag == 0) {
        /* Becoming inactive: propagate to children with (param_1=1,
         * param_2=0). */
        object_propagate_flag_to_children(object_handle, 1, 0);
        goto lab_00140017;
      } else {
        /* bit0 clear, flag!=0: re-check children block presence */
        if (!has_children)
          return;
        goto lab_00140017;
      }
    }
  }

lab_0014000a:
  if ((char)flag == 0)
    goto lab_00140017;
  /* flag != 0 and came from "has_children + bit0 set" path: skip datum
   * update if children block is absent. */
  if (!has_children)
    return;

lab_00140017: {
  /* Commit the datum-level flags. */
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  if ((char)flag == 0) {
    /* Mark as garbage: set bit 0 of obj->flags, clear hdr->unk_2 bit 1. */
    obj->flags |= 1;
    hdr->unk_2 &= (uint8_t)~0x02;
  } else {
    /* Unmark garbage: clear bit 0 of obj->flags, set hdr->unk_2 bit 1. */
    obj->flags &= ~(uint32_t)1;
    hdr->unk_2 |= 0x02;
  }
}
}

/* Walk the parent chain to the root object and copy its position and
 * forward vector to the output buffers (0x140070). Either output may
 * be NULL to skip copying. Position is at object offset 0x18 (3 floats),
 * forward direction at 0x3c (3 floats). */
void object_get_root_location(int object_handle, float *position_out,
                              float *direction_out)
{
  char *obj = (char *)object_get_and_verify_type(object_handle, -1);

  while (*(int *)(obj + 0xcc) != -1) {
    object_header_data_t *header = (object_header_data_t *)datum_get(
      *(data_t **)0x5a8d50, *(int *)(obj + 0xcc));
    obj = (char *)header->object;

    {
      int16_t type = *(int16_t *)(obj + 0x64);
      if ((1 << (type & 0x1f)) == 0) {
        char *msg = csprintf((char *)0x5ab100,
                             "got an object type we didn't expect "
                             "(expected one of 0x%08x but got #%d).",
                             -1, (int)type);
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
    }
  }

  if (position_out != NULL) {
    position_out[0] = *(float *)(obj + 0x18);
    position_out[1] = *(float *)(obj + 0x1c);
    position_out[2] = *(float *)(obj + 0x20);
  }

  if (direction_out != NULL) {
    direction_out[0] = *(float *)(obj + 0x3c);
    direction_out[1] = *(float *)(obj + 0x40);
    direction_out[2] = *(float *)(obj + 0x44);
  }
}

/*
 * object_get_location — returns the root object's 8-byte location pair.
 *
 * Resolves the topmost parent handle via FUN_0013d7f0(handle), verifies that
 * object with object_get_and_verify_type(root_handle, -1), then copies dwords
 * at offsets +0x48 and +0x4c into location_out.
 *
 * Confirmed: CALL 0x13d7f0 with object_handle, then CALL 0x13d680 with
 *            returned handle and mask -1.
 * Confirmed: MOV [obj+0x48] -> [location_out+0], MOV [obj+0x4c] ->
 *            [location_out+4].
 */
void object_get_location(int object_handle, void *location_out)
{
  int root_handle = object_get_root_parent(object_handle);
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(root_handle, -1);
  uint32_t *out = (uint32_t *)location_out;

  out[0] = obj->unk_72;
  out[1] = (uint32_t)obj->unk_76.value;
}

/*
 * object_set_region_count — update an object's interpolation region count.
 *
 * Copies the object's region node data from the "new" interpolation buffer
 * (at object+0x19c) into the "current" buffer (at object+0x198) via
 * object_header_block_reference_get, using csmemcpy with a size of
 * model_region_count * 32.
 *
 * If the requested region_count is >= (unk_134 - unk_132), it resets
 * unk_132 to 0 and sets unk_134 to the new region_count.
 *
 * Asserts that the object's type is NOT in the "cannot interpolate" mask
 * (types with bits 5-11 set: 0xFE0).
 *
 * Confirmed: 2 cdecl args (PUSH EDI + PUSH [EBP+0xc], ADD ESP via
 *            interleaved cleanup).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type, type_mask=-1).
 * Confirmed: CALL 0x1ba140 (tag_get) twice — 'obje' then 'mode'.
 * Confirmed: CALL 0x13dfc0 twice with pre-pushed args for csmemcpy.
 * Confirmed: assert string at 0x29bf80:
 *   "!TEST_FLAG(_object_mask_cannot_interpolate, object->object.type)"
 * Inferred: unk_408 / unk_412 at offsets 0x198/0x19c are interpolation
 *           buffer references (4 bytes each: {int16_t size, int16_t offset}).
 * Inferred: model tag + 0xb8 is the region count (int16_t).
 */
void object_set_region_count(int object_handle, int16_t region_count)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Look up the object definition tag ('obje'), then the model tag ('mode')
   * to get the number of model regions. */
  void *obje_tag = tag_get(0x6f626a65, *(int *)obj);
  void *mode_tag = tag_get(0x6d6f6465, *(int *)((char *)obje_tag + 0x34));
  int16_t model_region_count = *(int16_t *)((char *)mode_tag + 0xb8);

  /* Assert that this object type can be interpolated.
   * _object_mask_cannot_interpolate = 0xFE0 (bits 5 through 11). */
  if ((1 << (*(uint8_t *)((char *)obj + 0x64) & 0x1f)) & 0xfe0u) {
    display_assert(
      "!TEST_FLAG(_object_mask_cannot_interpolate, object->object.type)",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x5f3, 1);
    system_exit(-1);
  }

  /* Copy region node data from the "new" buffer to the "current" buffer.
   * The two references at obj+0x19c and obj+0x198 each describe a
   * {size, offset} pair into the object's dynamic data region. */
  {
    int copy_size = (int)model_region_count << 5;
    void *src =
      object_header_block_reference_get(object_handle, (char *)obj + 0x19c);
    void *dst =
      object_header_block_reference_get(object_handle, (char *)obj + 0x198);
    csmemcpy(dst, src, copy_size);
  }

  /* If the new region_count is large enough, reset unk_132 and store it. */
  if ((int)region_count >= (int)obj->unk_134 - (int)obj->unk_132) {
    obj->unk_132 = 0;
    obj->unk_134 = region_count;
  }
}

/*
 * object_adjust_interpolation_position — adds a delta vector to an object's
 * interpolation position within its node/region block.
 *
 * Validates that the object type is not in the "cannot interpolate" mask
 * (bits 5-11 = 0xFE0). If the object's unk_134 (int16_t at offset 0x86)
 * is nonzero, resolves the block reference at obj+0x198 and adds the delta
 * to the position vector at offsets +0x10, +0x14, +0x18 in that block.
 *
 * Confirmed: cdecl, 2 stack args (PUSH+PUSH pattern, ADD ESP cleanup).
 * Confirmed: CALL 0x0013d680 — object_get_and_verify_type(handle, -1).
 * Confirmed: TEST EAX,0xfe0 — same _object_mask_cannot_interpolate check as
 *   object_set_region_count.
 * Confirmed: CALL 0x0008d9f0 — display_assert with line 0x60a (1546).
 * Confirmed: CALL 0x0008e2f0 — system_exit(-1).
 * Confirmed: CMP word ptr [ESI+0x86],0x0 — checks unk_134 != 0.
 * Confirmed: ADD ESI,0x198 then PUSH ESI — block reference at obj+0x198.
 * Confirmed: CALL 0x0013dfc0 — object_header_block_reference_get.
 * Confirmed: FLD/FADD/FSTP float ptr at [EAX+0x10], [EAX+0x14], [EAX+0x18].
 */
void object_adjust_interpolation_position(int object_handle, vector3_t *delta)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Assert that this object type can be interpolated.
   * _object_mask_cannot_interpolate = 0xFE0 (bits 5 through 11). */
  if ((1 << (*(uint8_t *)((char *)obj + 0x64) & 0x1f)) & 0xfe0u) {
    display_assert(
      "!TEST_FLAG(_object_mask_cannot_interpolate, object->object.type)",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x60a, 1);
    system_exit(-1);
  }

  /* Only adjust if the object has interpolation data (unk_134 != 0). */
  if (obj->unk_134 != 0) {
    float *block = (float *)object_header_block_reference_get(
      object_handle, (char *)obj + 0x198);
    /* Add delta to the position vector at block offsets +0x10, +0x14, +0x18
     * (float indices 4, 5, 6). */
    block[4] += delta->x;
    block[5] += delta->y;
    block[6] += delta->z;
  }
}

/* Set region permutation by marker name (0x1402c0).
 * Searches model regions for a permutation whose name matches marker_name
 * (case-insensitive). If found, sets the object's region permutation index
 * at obj+0x130+region. If region_index is -1, searches all regions;
 * otherwise only the specified region. If param_4 is 0, forces the
 * permutation index to 0 regardless of the match position. */
void object_permute_region(int object_handle, const char *marker_name,
                           short region_index, char param_4)
{
  char *obj;
  char *obje_tag;
  int model_tag_index;
  char *mode_tag;
  int *regions_block;
  short region_iter;
  int region_i;
  char *region_element;
  int *permutations_block;
  short perm_iter;
  int perm_i;
  char *perm_element;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  obje_tag = (char *)tag_get(0x6f626a65, *(int *)obj);
  model_tag_index = *(int *)(obje_tag + 0x34);
  if (model_tag_index == -1)
    return;

  mode_tag = (char *)tag_get(0x6d6f6465, model_tag_index);
  regions_block = (int *)(mode_tag + 0xc4);
  region_i = 0;
  if (*regions_block <= 0)
    return;

  region_iter = 0;
  do {
    if (region_index == -1 || region_index == region_iter) {
      region_element =
        (char *)tag_block_get_element(regions_block, region_i, 0x4c);
      permutations_block = (int *)(region_element + 0x40);
      perm_iter = 0;
      if (*permutations_block > 0) {
        perm_i = 0;
        do {
          perm_element =
            (char *)tag_block_get_element(permutations_block, perm_i, 0x58);
          if (crt_stricmp(perm_element, marker_name) == 0) {
            *(char *)(obj + 0x130 + region_i) =
              param_4 ? (char)perm_iter : (char)0;
            break;
          }
          perm_iter = perm_iter + 1;
          perm_i = (int)perm_iter;
        } while (perm_i < *permutations_block);
      }
    }
    region_iter = region_iter + 1;
    region_i = (int)region_iter;
  } while (region_i < *regions_block);
}

/* Query an outgoing object function value (0x1403a0).
 * If function_index is -1, writes 1.0f and returns true.
 * Otherwise asserts index is in [0,4), writes the float at
 * object+0xe4+index*4 to out_value, and returns whether the
 * corresponding bit in the function-valid mask at object+0xd3 is set. */
bool object_get_function_value(int object_handle, short function_index,
                               void *out_value)
{
  char *obj;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if (function_index == -1) {
    *(int *)out_value = 0x3f800000;
    return true;
  }
  if (function_index < 0 || function_index >= 4) {
    display_assert(
      "function_index>=0 && function_index<NUMBER_OF_OUTGOING_OBJECT_FUNCTIONS",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x676, 1);
    system_exit(-1);
  }
  *(int *)out_value = *(int *)(obj + 0xe4 + (int)function_index * 4);
  return (*(unsigned char *)(obj + 0xd3) &
          (1 << ((unsigned char)function_index & 0x1f))) != 0;
}

/*
 * object_find_in_cluster — find objects in clusters matching type criteria.
 *
 * Begins a marker pass (object_reset_markers), then iterates over
 * cluster indices.  For each cluster, walks the collideable partition
 * (flags & 1 => 0x5a8d40) and/or noncollideable partition (flags & 2 =>
 * 0x5a8d30) using cluster_partition_iter_first/next (0x191a50/0x191660).
 *
 * Each found object is verified as having a valid type bit.  If the object's
 * marker_generation differs from the current global generation, it's stamped
 * with the new generation and added to the output array.  Objects whose
 * marker_generation already matches are skipped (already collected this pass).
 *
 * Returns when max_count objects are collected or all clusters are exhausted.
 * Ends the marker pass by clearing object_globals->marker_initialized.
 *
 * Parameters (cdecl, 5 args):
 *   flags            — bit 0: collideable, bit 1: noncollideable; 0=>all (-1)
 *   cluster_count    — number of cluster indices
 *   cluster_indices  — int16_t array of cluster indices
 *   max_count        — capacity of out_handles array (int16_t)
 *   out_handles      — output array for found object handles (int*)
 *
 * Confirmed: 5 cdecl params at [EBP+0x8..0x18].
 * Confirmed: CALL 0x13eb70 (object_reset_markers) with 0 pushed args.
 * Confirmed: flags==0 => overwritten with 0xffffffff.
 * Confirmed: cluster_partition_iter_first at 0x191a50 (3 cdecl args:
 *            partition, state, cluster_idx).
 * Confirmed: cluster_partition_iter_next at 0x191660 (2 cdecl args:
 *            partition, state).
 * Confirmed: Both iter functions return handle (int) or -1.
 * Confirmed: datum_get at 0x119320: result+8 = object_data_t*.
 * Confirmed: obj+0x64 is type (int16_t), obj+0x08 is marker_generation
 * (uint32_t). Confirmed: 0x5a8d28 is the global marker generation counter.
 * Confirmed: End-of-pass clears object_globals+0x01 (marker_initialized).
 * Confirmed: Returns uint16 count in AX.
 */
int16_t object_find_in_cluster(int flags, int16_t cluster_count,
                               int16_t *cluster_indices, int16_t max_count,
                               int *out_handles)
{
  int16_t found = 0;
  int i;

  if (flags == 0)
    flags = 0xFFFFFFFF;

  object_reset_markers();

  for (i = 0; i < cluster_count; i++) {
    int16_t cluster_idx = cluster_indices[i];
    int handle;
    int iter_state[2];

    /* Collideable partition (flags & 1) */
    if (flags & 1) {
      handle =
        cluster_partition_iter_first((void *)0x5a8d40, iter_state, cluster_idx);
      while (handle != -1) {
        object_header_data_t *header =
          (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
        object_data_t *obj = header->object;

        if (1 << ((uint8_t)obj->type & 0x1f) == 0) {
          char *msg =
            csprintf((char *)0x5ab100,
                     "got an object type we didn\\'t expect (expected one of "
                     "0x%08x but got #%d).",
                     -1, (int)obj->type);
          display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
          system_exit(-1);
        }

        if (!object_globals->object_marker_initialized) {
          display_assert("object_globals->object_marker_initialized",
                         "c:\\halo\\SOURCE\\objects\\objects.c", 0xdd7, 1);
          system_exit(-1);
        }

        if (obj->marker_generation != *(uint32_t *)0x5a8d28) {
          obj->marker_generation = *(uint32_t *)0x5a8d28;
          if (found >= max_count) {
            if (!object_globals->object_marker_initialized) {
              display_assert("object_globals->object_marker_initialized",
                             "c:\\halo\\SOURCE\\objects\\objects.c", 0xdba, 1);
              system_exit(-1);
            }
            object_globals->object_marker_initialized = 0;
            return found;
          }
          out_handles[found] = handle;
          found++;
        }
        handle = cluster_partition_iter_next((void *)0x5a8d40, iter_state);
      }
    }

    /* Noncollideable partition (flags & 2) */
    if (flags & 2) {
      int iter_state2[2];
      handle = cluster_partition_iter_first((void *)0x5a8d30, iter_state2,
                                            cluster_idx);
      while (handle != -1) {
        object_header_data_t *header =
          (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
        object_data_t *obj = header->object;

        if (1 << ((uint8_t)obj->type & 0x1f) == 0) {
          char *msg =
            csprintf((char *)0x5ab100,
                     "got an object type we didn\\'t expect (expected one of "
                     "0x%08x but got #%d).",
                     -1, (int)obj->type);
          display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
          system_exit(-1);
        }

        if (!object_globals->object_marker_initialized) {
          display_assert("object_globals->object_marker_initialized",
                         "c:\\halo\\SOURCE\\objects\\objects.c", 0xdd7, 1);
          system_exit(-1);
        }

        if (obj->marker_generation != *(uint32_t *)0x5a8d28) {
          obj->marker_generation = *(uint32_t *)0x5a8d28;
          if (found >= max_count) {
            if (!object_globals->object_marker_initialized) {
              display_assert("object_globals->object_marker_initialized",
                             "c:\\halo\\SOURCE\\objects\\objects.c", 0xdba, 1);
              system_exit(-1);
            }
            object_globals->object_marker_initialized = 0;
            return found;
          }
          out_handles[found] = handle;
          found++;
        }
        handle = cluster_partition_iter_next((void *)0x5a8d30, iter_state2);
      }
    }
  }

  if (!object_globals->object_marker_initialized) {
    display_assert("object_globals->object_marker_initialized",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0xdba, 1);
    system_exit(-1);
  }
  object_globals->object_marker_initialized = 0;
  return found;
}

/*
 * object_name_list_get_handle — look up an object handle by name-table index.
 *
 * Takes a 16-bit name index, validates it is in [0, 0x200), and returns
 * the object handle stored at object_name_list[index]. Returns 0xFFFFFFFF
 * (-1) if the index is out of range.
 *
 * Confirmed: range check [0, 0x200) via TEST AX,AX / CMP AX,0x200.
 * Confirmed: MOV ECX,[0x46f07c] — loads object_name_list pointer.
 * Confirmed: MOVSX EAX,AX — sign-extends index before array access.
 * Confirmed: MOV EAX,[ECX+EAX*4] — returns name_table[index].
 * Confirmed: OR EAX,0xFFFFFFFF on out-of-range — returns -1.
 */
int object_name_list_get_handle(int16_t index)
{
  if (index >= 0 && index < 0x200) {
    int *name_table = *(int **)0x46f07c;
    return name_table[(int)index];
  }
  return 0xffffffff;
}

/*
 * object_visible_to_any_player — check if an object is visible to any player.
 *
 * Returns true (1) if the object occupies a PVS-visible cluster AND is within
 * at least one player's field of view (or within the object's bounding sphere
 * distance from the player's head).
 *
 * The algorithm:
 *   1. Validate the object header flag (bit 0 of unk_2) and object flags
 *      (must have 0x800 set, must NOT have 0x200000 set).
 *   2. Iterate the object's clusters; for each, test visibility against the
 *      combined player PVS bitfield.
 *   3. If a visible cluster is found, iterate all players:
 *      a. Compute distance_squared from player head to object center.
 *      b. If dist_sq < radius_sq (player inside bounding sphere), visible.
 *      c. Otherwise, compute the half-angle subtended by the bounding sphere
 *         plus a PI/4 margin, and check if the object direction lies within
 *         the player's facing cone (dot product vs cos of half-angle).
 *
 * Confirmed: cdecl, 1 arg (object_handle at [EBP+8]).
 * Confirmed: Returns byte in AL (0 or 1).
 * Confirmed: CALL 0x119320 (datum_get) with object data table (0x5a8d50).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with mask -1 and 3.
 * Confirmed: CALL 0xba6c0 (players_get_combined_pvs) with no args.
 * Confirmed: CALL 0x13fe10 (object_get_first_cluster) with 2 cdecl args.
 * Confirmed: CALL 0x13d5f0 (object_get_next_cluster) with 2 cdecl args.
 * Confirmed: CALL 0x1198f0 (data_next_index) with player_data, prev_index.
 * Confirmed: CALL 0x1a9200 (unit_get_head_position) with unit_handle, &out.
 * Confirmed: CALL 0x13010 (normalize3d) with delta vector pointer.
 * Confirmed: PVS bit test pattern: (1 << (cluster & 0x1f)) & pvs[cluster >> 5].
 * Confirmed: Float at 0x254a58 = PI/4 (0.7854f).
 * Confirmed: Player unit handle at player_datum + 0x34.
 * Confirmed: Unit forward vector at unit_obj + 0x1E0 (vector3_t unk_480).
 * Confirmed: Object position at obj + 0x50 (unk_80/unk_84/unk_88).
 * Confirmed: Object bounding radius at obj + 0x5C (unk_92).
 */
int object_visible_to_any_player(int object_handle)
{
  object_header_data_t *header;
  object_data_t *obj;
  int *pvs;
  int16_t cluster_index;
  char iter_state[16];
  int player_index;
  char *player;
  float radius_sq;
  float head_pos[3];
  float dx, dy, dz;
  float dist_sq;
  float delta[3];
  float magnitude;
  float half_angle;
  float dot;
  char *unit_obj;
  int unit_handle;
  char result;

  result = 0;

  /* Validate object header and flags */
  header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);

  if (!(header->unk_2 & 1))
    return result;
  if (!(obj->flags & 0x800))
    return result;
  if (obj->flags & 0x200000)
    return result;

  /* Get combined PVS and iterate object clusters */
  pvs = (int *)players_get_combined_pvs();
  cluster_index = object_get_first_cluster(iter_state, object_handle);
  if (cluster_index == (int16_t)0xFFFF)
    return result;

  /* Check each cluster against PVS */
  for (;;) {
    int edx = (int)cluster_index;
    int bit_index = edx & 0x1f;
    int dword_index = edx >> 5;
    int bit_mask = 1 << bit_index;

    if (pvs[dword_index] & bit_mask)
      break;

    cluster_index = object_get_next_cluster(iter_state, object_handle);
    if (cluster_index == (int16_t)0xFFFF)
      return result;
  }

  /* Object is in a visible cluster — check per-player visibility */
  if (cluster_index == (int16_t)0xFFFF)
    return result;

  radius_sq = obj->unk_92 * obj->unk_92;

  player_index = data_next_index(*(data_t **)0x5aa6d4, -1);
  if (player_index == -1)
    return result;

  while (player_index != -1) {
    player = (char *)datum_get(*(data_t **)0x5aa6d4, player_index);
    unit_handle = *(int *)(player + 0x34);

    if (unit_handle == -1)
      goto next_player;

    /* Get player head position */
    unit_get_head_position(unit_handle, head_pos);

    /* Distance check: is player head within bounding sphere? */
    dx = obj->unk_80 - head_pos[0];
    dy = obj->unk_84 - head_pos[1];
    dz = obj->unk_88 - head_pos[2];
    dist_sq = dz * dz + dy * dy + dx * dx;

    if (dist_sq < radius_sq) {
      result = 1;
      return result;
    }

    /* FOV check: is object within player's viewing cone? */
    unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);

    delta[0] = obj->unk_80 - head_pos[0];
    delta[1] = obj->unk_84 - head_pos[1];
    delta[2] = obj->unk_88 - head_pos[2];
    magnitude = normalize3d(delta);

    /* half_angle = atan2(radius, magnitude) + PI/4 */
    half_angle = (float)(xbox_atan2((double)obj->unk_92, (double)magnitude) +
                         (double)0.7853981852531433f);

    /* dot product of normalized delta with unit forward vector */
    dot = delta[2] * *(float *)(unit_obj + 0x1E8) +
          delta[1] * *(float *)(unit_obj + 0x1E4) +
          delta[0] * *(float *)(unit_obj + 0x1E0);

    if (xbox_cosf(half_angle) < dot) {
      result = 1;
      return result;
    }

  next_player:
    player_index = data_next_index(*(data_t **)0x5aa6d4, player_index);
  }

  return result;
}

void object_pvs_activate(int param_1)
{
  int iVar1;

  iVar1 = *(int *)0x46f084;
  if (param_1 == -1) {
    *(short *)(iVar1 + 0x90) = 0;
    return;
  }
  *(short *)(iVar1 + 0x90) = 1;
  *(int *)(iVar1 + 0x94) = param_1;
}

void objects_scripting_set_scale(int param_1, int param_2, int16_t param_3)
{
  int iVar1;
  unsigned char cl;

  if (param_1 != -1) {
    iVar1 = (int)object_get_and_verify_type(param_1, 0xffffffff);
    *(int *)(iVar1 + 0x60) = param_2;
    cl = *(unsigned char *)(iVar1 + 0x64);
    if ((((unsigned int)1 << cl) & 0xfe0) == 0) {
      object_set_region_count(param_1, param_3);
    }
  }
}

/*
 * object_delete_internal — recursive object deletion implementation.
 *
 * Recursively deletes an object's child chain (obj+0xC8), and optionally
 * its sibling chain (obj+0xC4) when delete_sibling is nonzero. For each
 * object:
 *   1. If the game engine is running and the object is a weapon (type==2),
 *      asserts that it is not a flag (CTF flag weapon).
 *   2. Recursively deletes children and optionally siblings.
 *   3. Sets datum header bit 0x08 (pending deletion).
 *   4. If the object's tag definition has a children block (tag+0x34 != -1)
 *      and obj->flags bit 0 is clear, propagates deletion to attached
 *      children via FUN_0013ee60 (EAX=handle, args 1,0).
 *   5. Sets obj->flags bit 0 (deleted/inactive).
 *   6. Clears datum header bit 0x02 (active).
 *   7. Removes the object from the name list via FUN_0013eff0 (EDI=handle).
 *
 * Confirmed: cdecl, 2 stack args (PUSH+PUSH, ADD ESP,0x8 at recursive sites).
 * Confirmed: CALL 0x0013d680 — object_get_and_verify_type(handle, -1).
 * Confirmed: CALL 0x000a8e30 — game_engine_running(), no args, returns bool.
 * Confirmed: CMP word ptr [ESI+0x64],0x2 — checks object type == weapon.
 * Confirmed: CALL 0x000fb0c0 — weapon_is_flag(handle), 1 cdecl arg.
 * Confirmed: CALL 0x0008d9f0 — display_assert with line 0x33d (829).
 * Confirmed: CALL 0x0008e2f0 — system_exit(-1), NOT thunk_FUN_001029a0.
 * Confirmed: [ESI+0xC8] — child object handle for recursive delete.
 * Confirmed: [ESI+0xC4] — sibling object handle (conditional on
 * delete_sibling). Confirmed: OR AL,0x8 / MOV [EBX+0x2],AL — sets datum header
 * bit 0x08. Confirmed: MOV EAX,EDI before CALL 0x0013ee60 — EAX register arg =
 * handle. Confirmed: PUSH 0x0 / PUSH 0x1 — FUN_0013ee60 stack args (1, 0).
 * Confirmed: TEST byte ptr [ESI+0x4],0x1 — checks obj->flags bit 0.
 * Confirmed: OR dword ptr [ESI+0x4],0x1 — sets obj->flags bit 0.
 * Confirmed: AND CL,0xfd / MOV [EAX+0x2],CL — clears datum header bit 0x02.
 * Confirmed: CALL 0x0013eff0 — no stack args, EDI register arg = handle.
 */
void object_delete_internal(int object_handle, int delete_sibling)
{
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* If the game engine is running and this is a weapon, assert it's not a
   * flag (CTF flags should not be deleted this way). */
  if (game_engine_running() && obj->type == 2) {
    if (weapon_is_flag(object_handle)) {
      display_assert("!(weapon_is_flag(object_index))",
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x33d, 1);
      system_exit(-1);
    }
  }

  /* Recursively delete child objects (obj+0xC8). */
  if (obj->unk_200.value != -1) {
    object_delete_internal(obj->unk_200.value, 1);
  }

  /* Optionally recursively delete sibling objects (obj+0xC4). */
  if ((char)delete_sibling != 0 && obj->next_object_index.value != -1) {
    object_delete_internal(obj->next_object_index.value, 1);
  }

  /* Mark datum header with pending-deletion bit (0x08). */
  hdr->unk_2 |= 0x08;

  /* Re-fetch object pointer (may have been invalidated by recursive calls). */
  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Check if the object's tag definition has a children block. */
  {
    void *tag_def = tag_get(0x6f626a65, (int)obj->tag_index);
    if (*(int *)((char *)tag_def + 0x34) != -1 && (obj->flags & 1) == 0) {
      /* Propagate deletion to attached children. */
      object_propagate_flag_to_children(object_handle, 1, 0);
    }
  }

  /* Re-fetch datum header (recursive calls may have moved pool memory). */
  hdr = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);

  /* Set obj->flags bit 0 (deleted/inactive). */
  obj->flags |= 1;

  /* Clear datum header bit 0x02 (active). */
  hdr->unk_2 &= (uint8_t)~0x02;

  /* Remove the object from the name list. */
  object_remove_from_name_list(object_handle);
}

/*
 * object_delete — delete an object from the world.
 *
 * Thin wrapper around object_delete_internal with delete_sibling=0,
 * meaning only the target object and its children are deleted, not
 * its siblings in the object list.
 *
 * Confirmed: PUSH 0x0 / PUSH EAX / CALL 0x140bc0 / ADD ESP,0x8 — 2 cdecl args.
 */
void object_delete(int object_handle)
{
  object_delete_internal(object_handle, 0);
}

/*
 * object_connect_to_map — link an object into the BSP/collision world.
 *
 * If the object has a parent (parent_object_index != -1), it chains into the
 * parent's child list via next_object_index/unk_200 and marks the header as
 * attached. Otherwise, it resolves a BSP location (from the caller or by
 * probing the object's bounding position), inserts into the collision/BSP
 * structure, and optionally garbage-collects or activates the object based on
 * PVS visibility.
 *
 * Confirmed: CALL 0x119320 (datum_get) with objects global at 0x5a8d50.
 * Confirmed: assert "DATUM_INDEX_TO_IDENTIFIER(object_index)" at line 0x36f.
 * Confirmed: assert "!TEST_FLAG(object->object.flags,
 * _object_connected_to_map_bit)" at line 0x370. Confirmed: CALL 0x13d680
 * (object_get_and_verify_type) with PUSH -1. Confirmed: CALL 0x140bc0
 * (object_delete_internal) with delete_sibling=0. Confirmed: CALL 0xba6c0
 * (players_get_combined_pvs). Confirmed: offset 0xCC = parent_object_index,
 * 0xC8 = unk_200, 0xC4 = next_object_index. Confirmed: 0x5a8d40 and 0x5a8d30
 * are alternate object list pointers selected by flag 0x2000000.
 */
void object_connect_to_map(int object_handle, void *location)
{
  object_header_data_t *hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_data_t *obj = hdr->object;

  if ((object_handle & 0xffff0000) == 0) {
    display_assert("DATUM_INDEX_TO_IDENTIFIER(object_index)",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x36f, 1);
    system_exit(-1);
  }
  if ((obj->flags & 0x800) != 0) {
    display_assert(
      "!TEST_FLAG(object->object.flags, _object_connected_to_map_bit)",
      "c:\\halo\\SOURCE\\objects\\objects.c", 0x370, 1);
    system_exit(-1);
  }

  if (obj->parent_object_index.value != -1) {
    /* Child object: chain into parent's child linked list. */
    object_data_t *parent_obj = (object_data_t *)object_get_and_verify_type(
      obj->parent_object_index.value, -1);
    object_data_t *self_obj =
      (object_data_t *)object_get_and_verify_type(object_handle, -1);
    self_obj->next_object_index.value = parent_obj->unk_200.value;
    parent_obj->unk_200.value = (uint32_t)object_handle;
    hdr->unk_2 |= 0x80;
    *(int16_t *)((char *)obj + 0x4c) = -1;
  } else {
    /* Root object: resolve BSP location and insert into collision world. */
    uint32_t local_loc[2];
    uint32_t *loc;
    object_data_t *self_obj;
    void *obj_list;

    if (location == NULL) {
      scenario_location_from_point(local_loc, (char *)obj + 0x50);
      location = local_loc;
      if ((int16_t)local_loc[1] == -1) {
        scenario_location_from_point(local_loc, (char *)obj + 0x0c);
      }
    }

    loc = (uint32_t *)location;
    if ((int16_t)loc[1] == -1) {
      obj->flags |= 0x200000;
    } else {
      obj->unk_72 = loc[0];
      obj->unk_76.value = loc[1];
      hdr->unk_4 = (uint16_t)loc[1];
      obj->flags &= ~0x200000u;
    }

    hdr->unk_2 &= 0x7f;

    self_obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
    obj_list =
      (self_obj->flags & 0x2000000) ? (void *)0x5a8d40 : (void *)0x5a8d30;
    cluster_partition_add_object(obj_list, object_handle, (char *)obj + 0xbc,
                                 (char *)obj + 0x50, *(uint32_t *)&obj->unk_92,
                                 (char *)obj + 0x48);

    if ((hdr->unk_2 & 0x40) != 0) {
      if (hdr->unk_4 != 0xffff) {
        int16_t cluster = (int16_t)hdr->unk_4;
        int *pvs = (int *)players_get_combined_pvs();
        if ((pvs[cluster >> 5] & (1u << (cluster & 0x1f))) != 0) {
          object_activate(object_handle);
          goto done;
        }
      }
      if ((obj->flags & 0x80000) != 0) {
        object_delete_internal(object_handle, 0);
      }
    }
  }
done:
  obj->flags |= 0x800;
  hdr->unk_2 |= 0x20;
}

/*
 * object_get_node_matrix — return a pointer to a specific node's 4x3 matrix
 * within the object's node matrix block.
 *
 * Asserts that the object actually has the requested node via object_has_node.
 * Resolves the node matrix block reference at object+0x1A0, then indexes into
 * it by node_index * 0x34 (52 bytes per node matrix).
 *
 * Confirmed: CALL 0x13fef0 (object_has_node) with 2 stack args, TEST AL,AL.
 * Confirmed: assert string "object_has_node(object_index, node_index)" at
 *            0x29c070, file "c:\halo\SOURCE\objects\objects.c" at 0x29b91c,
 *            line 0x424.
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with PUSH -1.
 * Confirmed: ADD EAX,0x1A0 — node matrix block reference offset.
 * Confirmed: CALL 0x13dfc0 (object_header_block_reference_get).
 * Confirmed: MOVSX ECX,DI / IMUL ECX,ECX,0x34 — sign-extended int16_t index
 *            multiplied by 52.
 * Confirmed: ADD EAX,ECX — final pointer = base + node_index * 0x34.
 */
void *object_get_node_matrix(int object_handle, int16_t node_index)
{
  object_data_t *obj;
  char *nodes;

  if (!object_has_node(object_handle, node_index)) {
    display_assert("object_has_node(object_index, node_index)",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x424, 1);
    system_exit(-1);
  }
  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  nodes = (char *)object_header_block_reference_get(object_handle,
                                                    (void *)&obj->unk_416);
  return nodes + (int)node_index * 0x34;
}

/*
 * object_get_markers_by_string_id — find markers on an object by name string,
 * returning a count of matched markers.
 *
 * Delegates to model_find_markers (0x124730) which searches the object's
 * animation graph tag for markers matching marker_name. If no markers are
 * found and marker_name is NULL or empty, fills out_markers[0] with a default
 * identity transform and the node-0 matrix, returning 1.
 *
 * When sVar1 == 0 (no markers found from the model search):
 *   - Asserts max_count > 0
 *   - Zeros the node_index at out_markers[0]+0x00
 *   - Initializes a 52-byte identity transform at out_markers[0]+0x04
 *   - Copies the 52-byte node-0 matrix into out_markers[0]+0x38
 *   - If the object has the 0x1000 flag (mirrored), negates the second row
 *     of the node matrix (offsets +0x48, +0x4C, +0x50 in the marker)
 *   - Returns 1 if marker_name is non-NULL and points to an empty string
 *
 * Confirmed: PUSH -1 / PUSH ESI / CALL 0x13d680 — object_get_and_verify_type.
 * Confirmed: PUSH 0x6f626a65 — tag_get('obje', ...).
 * Confirmed: LEA EDX,[EBX+0x130] — object nodes (unk_304) passed to model
 * search. Confirmed: ADD EAX,0x1A0 — node matrix block reference at
 * object+0x1A0. Confirmed: SHR ECX,0xC / AND ECX,0xFFFFFF01 — flag extraction
 * from obj->flags. Confirmed: REP MOVSD with ECX=0xD — copies 52-byte node
 * matrix (0x34 bytes). Confirmed: TEST AH,0x10 — checks flags bit 12 (0x1000)
 * for mirroring. Confirmed: FCHS on floats at [EAX+0x48], [EAX+0x4C],
 * [EAX+0x50]. Confirmed: ADD ESP,0x44 — cleans all 17 dwords across 5 cdecl
 * calls.
 */
int16_t object_get_markers_by_string_id(int object_handle, void *marker_name,
                                        void *out_markers, int max_count)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  void *tag_data = tag_get(0x6f626a65, obj->tag_index);
  object_data_t *obj2 =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  void *node_matrices =
    object_header_block_reference_get(object_handle, (char *)obj2 + 0x1a0);

  /* model_find_markers (0x124730): search animation graph for named markers.
   * 9 args: anim_graph_data, marker_name, object_nodes, zero, -1,
   *         node_matrices, flags, out_markers, max_count */
  uint32_t mirror_flags = (obj->flags >> 12) & 0xffffff01;
  int16_t result = ((int16_t(*)(int, void *, void *, int, int, void *, uint32_t,
                                void *, int))0x124730)(
    *(int *)((char *)tag_data + 0x34), marker_name, (char *)obj + 0x130, 0, -1,
    node_matrices, mirror_flags, out_markers, max_count);

  if (result != 0)
    return result;

  /* No markers found — fill in a default marker if possible. */
  if ((int16_t)max_count < 1) {
    display_assert("maximum_marker_count>0",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x459, 1);
    system_exit(-1);
  }

  /* Zero the node index (int16_t at offset 0x00). */
  *(int16_t *)out_markers = 0;

  /* Initialize identity transform at out_markers+0x04 (52 bytes). */
  ((void (*)(void *))0x1090e0)((char *)out_markers + 4);

  /* Copy node-0 matrix (52 bytes / 13 dwords) into out_markers+0x38. */
  {
    void *node_mat = object_get_node_matrix(object_handle, 0);
    qmemcpy((char *)out_markers + 0x38, node_mat, 0x34);
  }

  /* If the object is mirrored (flags bit 12), negate the second row of the
   * node matrix within the marker result (offsets +0x48, +0x4C, +0x50). */
  if ((obj->flags & 0x1000) != 0) {
    *(float *)((char *)out_markers + 0x48) =
      -*(float *)((char *)out_markers + 0x48);
    *(float *)((char *)out_markers + 0x4c) =
      -*(float *)((char *)out_markers + 0x4c);
    *(float *)((char *)out_markers + 0x50) =
      -*(float *)((char *)out_markers + 0x50);
  }

  /* If marker_name is non-NULL and points to an empty string, return 1. */
  if (marker_name != NULL && *(char *)marker_name == '\0')
    return 1;

  return 0;
}

/*
 * object_compute_child_marker_position — given an object pointer, a child
 * marker (containing a matrix4x3 at offset 0x38), and a destination matrix,
 * computes the child marker's position relative to the object and writes the
 * resulting position, forward, and up vectors back into the object.
 *
 * Algorithm:
 *   1. Build a matrix4x3 from the object's forward, up, position vectors.
 *   2. Invert it.
 *   3. Multiply the inverted matrix by the child marker's matrix (at +0x38).
 *   4. Invert the result.
 *   5. Multiply destination_matrix by the inverted result.
 *   6. Extract position, forward, and up from the product back to the object.
 *   7. Re-orthogonalize up via cross(cross(forward, up), forward).
 *   8. Normalize forward and up.
 *
 * Confirmed: 3 cdecl args (object, child_marker, destination_matrix).
 * Confirmed: void return — no caller checks EAX.
 * Confirmed: assert strings match "object", "child_marker",
 *            "destination_matrix", "valid_real_matrix4x3(destination_matrix)".
 * Confirmed: source file "c:\\halo\\SOURCE\\objects\\objects.c", lines
 * 0x495–0x499. Confirmed: CALL 0x10a110 (matrix4x3_from_forward_up_position).
 * Confirmed: CALL 0x109150 (matrix_inverse) x2.
 * Confirmed: CALL 0x109850 (matrix4x3_multiply) x2.
 * Confirmed: CALL 0x13010  (normalize3d) x2.
 * Confirmed: CALL 0xf6d00  (valid_real_matrix4x3) for dest_matrix assertion.
 * Confirmed: ADD ESP,0x40 cleans all 16 cdecl arg dwords at once.
 */
void object_compute_child_marker_position(void *object, void *child_marker,
                                          void *dest_matrix)
{
  typedef void (*matrix4x3_from_fup_fn)(void *out, float *pos, float *fwd,
                                        float *up);
  typedef void (*matrix_inverse_fn)(void *src, void *dst);
  typedef void (*matrix4x3_multiply_fn)(void *out, void *a, void *b);
  typedef int (*valid_real_matrix4x3_fn)(void *mat);

  float local_mat[13]; /* 0x34 bytes: scale + forward + left + up + position */
  float inv_mat[13]; /* 0x34 bytes */
  float *obj_position;
  float *obj_forward;
  float *obj_up;
  float fwd_x, fwd_y, fwd_z;
  float up_x, up_y, up_z;
  float left_x, left_y, left_z;

  assert_halt(object != NULL);
  assert_halt(child_marker != NULL);
  assert_halt(dest_matrix != NULL);
  assert_halt(((valid_real_matrix4x3_fn)0xf6d00)(dest_matrix));

  obj_position = (float *)((char *)object + 0xc);
  obj_forward = (float *)((char *)object + 0x24);
  obj_up = (float *)((char *)object + 0x30);

  /* Build a matrix4x3 from the object's orientation and position */
  ((matrix4x3_from_fup_fn)0x10a110)(local_mat, obj_position, obj_forward,
                                    obj_up);

  /* Invert it */
  ((matrix_inverse_fn)0x109150)(local_mat, inv_mat);

  /* Multiply by the child marker's matrix at offset 0x38 */
  ((matrix4x3_multiply_fn)0x109850)(inv_mat, (char *)child_marker + 0x38,
                                    inv_mat);

  /* Invert the result */
  ((matrix_inverse_fn)0x109150)(inv_mat, inv_mat);

  /* Multiply dest_matrix by the inverted result, storing in local_mat */
  ((matrix4x3_multiply_fn)0x109850)(dest_matrix, inv_mat, local_mat);

  /* Extract position back to object (offsets 0x28..0x30 in matrix = indices
   * 10..12) */
  obj_position[0] = local_mat[10];
  obj_position[1] = local_mat[11];
  obj_position[2] = local_mat[12];

  /* Extract forward back to object (offsets 0x04..0x0c in matrix = indices
   * 1..3) */
  obj_forward[0] = local_mat[1];
  obj_forward[1] = local_mat[2];
  obj_forward[2] = local_mat[3];

  /* Re-orthogonalize up: left = cross(forward, up_from_matrix),
   * then up = cross(left, forward).
   * up_from_matrix is at indices 7..9 (offsets 0x1c..0x24). */
  fwd_x = local_mat[1];
  fwd_y = local_mat[2];
  fwd_z = local_mat[3];
  up_x = local_mat[7];
  up_y = local_mat[8];
  up_z = local_mat[9];

  /* left = cross(forward, up) */
  left_x = fwd_y * up_z - fwd_z * up_y;
  left_y = fwd_z * up_x - up_z * fwd_x;
  left_z = up_y * fwd_x - fwd_y * up_x;

  /* up_new = cross(left, forward) */
  obj_up[0] = left_y * fwd_z - left_z * fwd_y;
  obj_up[1] = left_z * fwd_x - fwd_z * left_x;
  obj_up[2] = fwd_y * left_x - left_y * fwd_x;

  normalize3d(obj_forward);
  normalize3d(obj_up);
}

/*
 * object_detach_from_parent — detach an object from its parent in the
 * object hierarchy.
 *
 * Retrieves both the child and parent object data, disconnects the child
 * from the map, then re-computes its orientation in world space using the
 * parent's node matrix.  After updating position/orientation/up vectors from
 * the parent, clears the node index (0xFF) and parent handle (-1), then
 * reconnects to the map.
 *
 * Finally, sets the "connected to cluster" flag (bit 0 of header+0x02) if
 * the object is not already connected, doesn't have the 0x100000 flag, and
 * has no parent.
 *
 * Confirmed: object_get_and_verify_type(handle, -1) for both child and parent.
 * Confirmed: object_disconnect_from_map(handle), then recompute world matrix
 *            via object_get_node_matrix(parent_handle, node_index).
 * Confirmed: matrix4x3_identity_with_position, matrix_from_forward_and_up,
 *            matrix4x3_multiply used to transform orientation.
 * Confirmed: Copies 3 floats at +0x18, +0x1C, +0x20 and +0x3C, +0x40, +0x44
 *            from parent to child.
 * Confirmed: Sets node_index (byte at +0xD0) = 0xFF, parent (int at +0xCC) =
 * -1. Confirmed: datum_get + flag check on header+0x02 bit 0, object+0x04 &
 * 0x100000, object+0xCC == -1 to set connected flag.
 */
void object_detach_from_parent(int object_handle)
{
  object_data_t *child;
  object_data_t *parent;
  void *node_matrix;
  float child_position[13];
  float child_orientation[13];
  float result[13];
  object_header_data_t *header;

  child = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  parent = (object_data_t *)object_get_and_verify_type(
    child->parent_object_index.value, -1);

  object_disconnect_from_map(object_handle);

  node_matrix =
    object_get_node_matrix(child->parent_object_index.value,
                           (int16_t) * (int8_t *)((char *)child + 0xd0));

  matrix4x3_identity_with_position(child_position, (float *)&child->unk_12);
  matrix_from_forward_and_up(child_orientation, (float *)&child->unk_36,
                             (float *)&child->unk_48);
  matrix4x3_multiply((float *)node_matrix, child_position, result);
  matrix4x3_multiply(result, child_orientation, result); /* dup-args-ok */
  matrix4x3_decompose(result, (float *)&child->unk_12, (float *)&child->unk_36,
                      (float *)&child->unk_48);

  child->unk_24 = parent->unk_24;
  child->unk_60 = parent->unk_60;

  *(int8_t *)((char *)child + 0xd0) = -1;
  child->parent_object_index.value = NONE;

  object_connect_to_map(object_handle, NULL);

  header =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  child = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  if (!(header->unk_2 & 1) && !(child->flags & 0x100000) &&
      child->parent_object_index.value == NONE) {
    header->unk_2 |= 1;
  }
}

/*
 * object_get_world_position — retrieve the world-space position of an object.
 *
 * If the object has no parent (parent_object_index == -1), copies the local
 * position vector (obj+0x0C) directly to out_position.
 *
 * If the object is attached to a parent, retrieves the parent's node matrix
 * via object_get_node_matrix (using the sign-extended node index byte at
 * obj+0xD0) and transforms the local position through that matrix via
 * matrix_transform_point.
 *
 * Confirmed: PUSH -1, PUSH EAX — object_get_and_verify_type(handle, -1).
 * Confirmed: CMP EAX,-1 — checks parent_object_index at obj+0xCC.
 * Confirmed: MOVSX CX, byte ptr [ESI+0xD0] — sign-extends node index byte.
 * Confirmed: ADD ESP,0x14 cleans 5 args (2 + 3 from two cdecl calls).
 * Confirmed: MOV EAX, EDI — returns out_position pointer.
 */
vector3_t *object_get_world_position(int object_handle, vector3_t *out_position)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  void *node_mat;

  if (obj->parent_object_index.value == NONE) {
    /* No parent — local position is the world position */
    *out_position = obj->unk_12;
    return out_position;
  }

  /* Parented — transform local position through parent's node matrix */
  node_mat = object_get_node_matrix(obj->parent_object_index.value,
                                    (int16_t) * (int8_t *)((char *)obj + 0xd0));
  matrix_transform_point((float *)node_mat, (float *)&obj->unk_12,
                         (float *)out_position);
  return out_position;
}

/*
 * object_get_orientation — get an object's forward and/or up orientation
 * vectors in world space.
 *
 * If the object has no parent (parent_object_index == -1), copies the local
 * forward (obj+0x24) and up (obj+0x30) vectors directly.
 * If parented, transforms both vectors through the parent's node matrix via
 * matrix_transform_vector (0x109680).
 *
 * When both out_forward and out_up are provided, validates that they form
 * perpendicular unit axes via valid_real_normal3d_perpendicular (0x84a70).
 *
 * Confirmed: 3 cdecl args at [EBP+0x8..0x10].
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0x140eb0 (object_get_node_matrix) with (parent_handle,
 *            sign-extended byte at obj+0xD0).
 * Confirmed: CALL 0x109680 (matrix_transform_vector) with (node_matrix,
 *            src_vector, out_vector) — 3 cdecl args each call.
 * Confirmed: CALL 0x84a70 (valid_real_normal3d_perpendicular) with
 *            (out_forward, out_up).
 * Confirmed: Assertion at line 0x5b6, strings "forward" (0x28cb2c) and
 *            "up" (0x28cb28), format at 0x267490.
 */
/* 0x141360 */
void object_get_orientation(int object_handle, float *out_forward,
                            float *out_up)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  if (obj->parent_object_index.value == NONE) {
    /* No parent — copy local forward and up vectors directly */
    if (out_forward != NULL) {
      out_forward[0] = ((float *)&obj->unk_36)[0];
      out_forward[1] = ((float *)&obj->unk_36)[1];
      out_forward[2] = ((float *)&obj->unk_36)[2];
    }
    if (out_up != NULL) {
      out_up[0] = ((float *)&obj->unk_48)[0];
      out_up[1] = ((float *)&obj->unk_48)[1];
      out_up[2] = ((float *)&obj->unk_48)[2];
    }
  } else {
    /* Parented — transform through parent's node matrix */
    void *node_mat =
      object_get_node_matrix(obj->parent_object_index.value,
                             (int16_t) * (int8_t *)((char *)obj + 0xd0));

    if (out_forward != NULL) {
      matrix_transform_vector((float *)node_mat, (float *)&obj->unk_36,
                              out_forward);
    }
    if (out_up != NULL) {
      matrix_transform_vector((float *)node_mat, (float *)&obj->unk_48, out_up);
    }
  }

  /* Validate perpendicularity if both vectors were requested */
  if (out_forward != NULL && out_up != NULL) {
    if (!valid_real_normal3d_perpendicular(out_forward, out_up)) {
      char *msg = csprintf(
        (char *)0x5ab100,
        "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
        "forward", "up", (double)out_forward[0], (double)out_forward[1],
        (double)out_forward[2], (double)out_up[0], (double)out_up[1],
        (double)out_up[2]);
      display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x5b6, 1);
      system_exit(-1);
    }
  }
}

/*
 * object_get_world_matrix — build a 4x3 world-space matrix for an object.
 *
 * Constructs the matrix from the object's position (obj+0xc), forward
 * vector (obj+0x24), and up vector (obj+0x30) via FUN_0010a110 (which
 * calls matrix_from_forward_and_up then copies position to offset 0x28).
 *
 * If the object has a parent (parent_object_index at obj+0xcc != -1),
 * retrieves the parent's node matrix via FUN_00140eb0 (using the node
 * index byte at obj+0xd0) and multiplies it with the local matrix via
 * FUN_00109850 (matrix_multiply), storing the result in-place.
 *
 * Confirmed: PUSH -1, PUSH EAX — object_get_and_verify_type(handle, -1).
 * Confirmed: ADD ESP,0x18 cleans 6 args (2 + 4 from two cdecl calls).
 * Confirmed: MOVSX CX, byte ptr [ESI+0xd0] — sign-extends node index.
 * Confirmed: ADD ESP,0x14 cleans 5 args (2 + 3 from two cdecl calls).
 * Confirmed: MOV EAX, EDI — returns out_matrix pointer.
 */
void *object_get_world_matrix(int object_handle, void *out_matrix)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);

  /* Build local matrix from position, forward, up */
  ((void (*)(void *, float *, float *, float *))0x10a110)(
    out_matrix, (float *)((char *)obj + 0xc), (float *)((char *)obj + 0x24),
    (float *)((char *)obj + 0x30));

  /* If parented, multiply by parent's node matrix */
  if (obj->parent_object_index.value != NONE) {
    void *node_mat =
      object_get_node_matrix(obj->parent_object_index.value,
                             (int16_t) * (int8_t *)((char *)obj + 0xd0));
    matrix4x3_multiply((float *)node_mat, (float *)out_matrix,
                       (float *)out_matrix); /* dup-args-ok */
  }

  return out_matrix;
}

/*
 * object_find_in_radius — find objects of a given type within a spherical area.
 *
 * Uses the structure system to identify candidate clusters, then iterates
 * through objects in those clusters, filtering by type_mask and distance.
 * Objects within (obj_effective_radius + search_radius) of the search position
 * are collected into out_handles.  Returns the count of found objects.
 *
 * Parameters (cdecl, 7 args):
 *   flags            — passed to object_find_in_cluster
 *   type_mask        — bit mask of object types to include (0 → all types)
 *   cluster_info     — pointer to a cluster location struct; word at +4 is
 *                       the cluster count passed to structure_find_in_cluster
 *   position         — float[3] search center (must be non-NULL)
 *   radius           — search radius, added to each object's effective radius
 *   out_handles      — output array for found object handles (must be non-NULL)
 *   max_count        — maximum number of handles to collect
 *
 * Confirmed: 7 cdecl params at [EBP+0x8..0x20].
 * Confirmed: param_3 (EBX) is a struct pointer, word at +4 = cluster count.
 * Confirmed: param_4 (ESI) is the float* position, used in distance math.
 * Confirmed: CALL 0x199230 with 5 args: (cluster_count_word, position, radius,
 *            0x200, cluster_indices_ptr).
 * Confirmed: CALL 0x140420 with 5 args: (flags, cluster_count, cluster_indices,
 *            0x800, object_indices_ptr). Max counts are hardcoded 512 and 2048.
 * Confirmed: Distance check uses obj+0x50/0x54/0x58 vs position, and
 *            obj+0x5C as effective object radius.
 * Confirmed: Returns short (count of found objects).
 * Confirmed: Only type_mask gets the 0 → -1 treatment; flags is NOT checked.
 * Confirmed: Assert strings: "location" (0x29c114), "center" (0x253f0c),
 *            "object_indices" (0x29c104) at lines 0x6f3, 0x6f4, 0x6f5.
 * Confirmed: Inner assert uses csprintf with full format at 0x29b940:
 *            "got an object type we didn't expect (expected one of 0x%08x
 *             but got #%d)." with args (-1, type).
 * Confirmed: Loop iterator i is int16_t (BX register, CMP BX).
 */
/* 0x1415f0 */
int16_t object_find_in_radius(int flags, unsigned int type_mask,
                              void *cluster_info, float *position, float radius,
                              int *out_handles, int16_t max_count)
{
  int16_t found_count = 0;
  int16_t iter_count;
  int16_t i;

  static int16_t cluster_indices[512];
  static int object_indices[2048];

  if (cluster_info == NULL) {
    display_assert("location", "c:\\halo\\SOURCE\\objects\\objects.c", 0x6f3,
                   1);
    system_exit(-1);
  }
  if (position == NULL) {
    display_assert("center", "c:\\halo\\SOURCE\\objects\\objects.c", 0x6f4, 1);
    system_exit(-1);
  }
  if (out_handles == NULL) {
    display_assert("object_indices", "c:\\halo\\SOURCE\\objects\\objects.c",
                   0x6f5, 1);
    system_exit(-1);
  }

  if (type_mask == 0)
    type_mask = 0xFFFFFFFF;

  iter_count =
    structure_find_in_cluster(*(uint16_t *)((char *)cluster_info + 4), position,
                              radius, 512, cluster_indices);

  iter_count = object_find_in_cluster(flags, iter_count, cluster_indices, 2048,
                                      object_indices);

  for (i = 0; i < iter_count && found_count < max_count; i++) {
    int handle = object_indices[i];
    object_header_data_t *header =
      (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, handle);
    object_data_t *obj = header->object;

    if ((1 << ((uint8_t)obj->type & 0x1f)) == 0) {
      char *msg = csprintf((char *)0x5ab100,
                           "got an object type we didn't expect "
                           "(expected one of 0x%08x but got #%d).",
                           (int)-1, (int)obj->type);
      display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }

    if ((type_mask & (1 << ((uint8_t)obj->type & 0x1f))) != 0) {
      float dx = obj->unk_80 - position[0];
      float dy = obj->unk_84 - position[1];
      float dz = obj->unk_88 - position[2];
      float effective_radius = obj->unk_92 + radius;

      if (dx * dx + dy * dy + dz * dz <= effective_radius * effective_radius) {
        out_handles[found_count] = handle;
        found_count++;
      }
    }
  }

  return found_count;
}

/* Type-cast helpers for object_compute_node_matrices — kept at file scope for
 * C89 compliance */
typedef void (*animation_set_default_fn)(void *model_tag, void *anim_data);
typedef void (*animation_decode_fn)(void *model_tag, void *anim_entry,
                                    int frame_index, void *anim_data);
typedef void (*animation_overlay_keyframe_fn)(void *anim_entry,
                                              float frame_value,
                                              void *anim_data);
typedef void (*animation_overlay_interpolate_fn)(void *anim_entry,
                                                 int frame_index,
                                                 void *anim_data,
                                                 void *node_data);
typedef void (*overlay_adjust_fn)(int object_handle, void *anim_data);
typedef void (*anim_interpolate_fn)(uint16_t node_count, void *interp_data,
                                    void *anim_data, int16_t frame_index,
                                    int16_t frame_count);
typedef int (*valid_real_vectors_fn)(float *fwd, float *left, float *up);
typedef int (*valid_real_matrix4x3_fn)(float *m);
typedef int (*valid_fwd_and_up_fn)(float *fwd, float *up);
typedef void (*matrix_4x3_multiply_fn)(float *a, float *b, float *out);
typedef void (*matrix_4x3_from_point_fn)(float *out, float *point);
typedef void (*model_node_set_default_fn)(float *out, void *anim_data);

/*
 * object_compute_node_matrices — compute the full node matrix hierarchy for an
 * object, transforming each node from local (animation) space into world space.
 *
 * If the object definition has no model (tag+0x34 == -1), builds a trivial
 * single-node matrix from the object's forward, up, and position vectors.
 *
 * Otherwise:
 *  1. Resolves the object type definition and model tag.
 *  2. Gets the parent node matrix if the object is attached to a parent.
 *  3. Decompresses the base animation pose into per-node local transforms.
 *  4. Applies overlay animations from the object definition's animation graph.
 *  5. Scales the root animation data if the object has a non-zero scale factor.
 *  6. Calls type-specific overlay adjustments via object_type_definition.
 *  7. Interpolates node matrices for objects with interpolation data.
 *  8. Validates the object's position and orientation vectors.
 *  9. Walks the node hierarchy (BFS via child/sibling indices), composing each
 *     node's local transform with its parent's world matrix.
 * 10. Applies an origin offset from the model tag and writes the bounding
 *     sphere radius.
 *
 * The bulk of the assembly is assertion/validation code that checks matrix
 * components for NaN/Inf and perpendicularity, expanded from the
 * assert_valid_real_matrix4x3 macro.
 *
 * Confirmed: 1 cdecl arg (object_handle). void return.
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (-1, handle).
 * Confirmed: CALL 0x1ba140 (tag_get) with 'obje' and object definition index.
 * Confirmed: CALL 0x13dfc0 (object_header_block_reference_get) for node matrix
 *            and animation data block references.
 * Confirmed: 0xfe0 type mask = bits 5..11 (_object_mask_cannot_interpolate).
 * Confirmed: CALL 0x109500 (model_node_matrices_set_default) for default pose.
 * Confirmed: CALL 0x109850 (matrix_4x3_multiply) for composing transforms.
 * Confirmed: CALL 0x109280 (matrix_4x3_from_point) for translation matrices.
 * Confirmed: CALL 0x109e10 (matrix_from_forward_and_up) for orientation.
 * Confirmed: CALL 0x109590 (matrix_transform_point) for origin offset.
 * Confirmed: SUB ESP,0xa44 — large stack frame for animation data and node
 *            queue.
 */
void object_compute_node_matrices(int object_handle)
{
  /* MSVC original: SUB ESP,0xa44. Pad to match so unported callees that
     read from overlapping MSVC stack offsets see valid memory. */
  volatile char _msvc_frame_pad[92];
  object_data_t *obj;
  void *object_tag;
  float *node_matrices;
  uint8_t obj_type_byte;
  int cannot_interpolate;
  void *anim_data;
  char anim_data_stack[2048];

  (void)_msvc_frame_pad;

  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  object_tag = tag_get(0x6f626a65, *(int *)obj);
  node_matrices = (float *)object_header_block_reference_get(
    object_handle, (void *)((char *)obj + 0x1a0));

  /* Objects with type bits 5..11 set cannot interpolate and use a stack
   * buffer for animation data; others use the block reference at +0x19c. */
  obj_type_byte = *(uint8_t *)((char *)obj + 0x64);
  cannot_interpolate = ((1 << (obj_type_byte & 0x1f)) & 0xfe0u) != 0;

  if (cannot_interpolate) {
    anim_data = anim_data_stack;
  } else {
    anim_data = object_header_block_reference_get(
      object_handle, (void *)((char *)obj + 0x19c));
  }

  if (*(int *)((char *)object_tag + 0x34) == -1) {
    /* No model — build a trivial matrix from object vectors */
    node_matrices[0] = 1.0f; /* scale */
    node_matrices[1] = *(float *)((char *)obj + 0x24); /* forward.x */
    node_matrices[2] = *(float *)((char *)obj + 0x28); /* forward.y */
    node_matrices[3] = *(float *)((char *)obj + 0x2c); /* forward.z */
    node_matrices[7] = *(float *)((char *)obj + 0x30); /* up.x */
    node_matrices[8] = *(float *)((char *)obj + 0x34); /* up.y */
    node_matrices[9] = *(float *)((char *)obj + 0x38); /* up.z */
    /* left = up x forward */
    node_matrices[4] =
      node_matrices[8] * node_matrices[3] - node_matrices[9] * node_matrices[2];
    node_matrices[5] =
      node_matrices[9] * node_matrices[1] - node_matrices[3] * node_matrices[7];
    node_matrices[6] =
      node_matrices[7] * node_matrices[2] - node_matrices[8] * node_matrices[1];
    /* position */
    node_matrices[10] = *(float *)((char *)obj + 0x0c);
    node_matrices[11] = *(float *)((char *)obj + 0x10);
    node_matrices[12] = *(float *)((char *)obj + 0x14);
  } else {
    /* Has a model — full animation pipeline */
    void *model_tag;
    float *parent_node_mat;
    uint8_t override_decompressor;
    int anim_tag_index;

    ((object_type_validate_fn)0x13c100)(*(int16_t *)((char *)obj + 0x64));
    model_tag = tag_get(0x6d6f6465, *(int *)((char *)object_tag + 0x34));

    /* Get parent node matrix if attached to a parent object */
    parent_node_mat = NULL;
    if (obj->parent_object_index.value != -1) {
      parent_node_mat = (float *)object_get_node_matrix(
        obj->parent_object_index.value,
        (int16_t) * (int8_t *)((char *)obj + 0xd0));
    }

    override_decompressor = 0;

    /* Decode animation pose into anim_data */
    anim_tag_index = *(int *)((char *)obj + 0x7c);
    if (anim_tag_index == -1 || *(int16_t *)((char *)obj + 0x80) == -1) {
      /* No animation graph or no animation index — use default pose */
      ((animation_set_default_fn)0x123aa0)(model_tag, anim_data);
    } else {
      void *anim_tag = tag_get(0x616e7472, anim_tag_index);
      void *anim_entry = tag_block_get_element(
        (char *)anim_tag + 0x74, (int)*(int16_t *)((char *)obj + 0x80), 0xb4);

      if ((*(char *)((char *)obj + 0x4) < 0) &&
          (0 < *(int16_t *)((char *)anim_entry + 0x22))) {
        /* Object flag bit 7 set and animation has frames — compute
         * frame from game_time + object_handle modulo frame count */
        int time = game_time_get();
        uint32_t frame_index =
          (uint32_t)(time + object_handle) %
          (uint32_t)(int)*(int16_t *)((char *)anim_entry + 0x22);
        if ((int16_t)frame_index < 0) {
          display_assert("frame_index>=0",
                         "c:\\halo\\SOURCE\\objects\\objects.c", 0xa90, 1);
          system_exit(-1);
        }
        ((animation_decode_fn)0x121d60)(model_tag, anim_entry,
                                        (int)(int16_t)frame_index, anim_data);
      } else {
        /* Use the stored frame index at obj+0x82 */
        int16_t frame_idx = *(int16_t *)((char *)obj + 0x82);
        ((animation_decode_fn)0x121d60)(model_tag, anim_entry, (int)frame_idx,
                                        anim_data);
      }
      override_decompressor =
        (*(uint8_t *)((char *)anim_entry + 0x3a) >> 1) & 1;
    }

    /* Apply overlay animations from object definition's animation graph */
    if (*(int *)((char *)object_tag + 0x44) != -1) {
      void *overlay_anim_tag =
        tag_get(0x616e7472, *(int *)((char *)object_tag + 0x44));
      int overlay_count = *(int *)overlay_anim_tag;
      int16_t overlay_idx = 0;
      if (overlay_count > 0) {
        int i = 0;
        do {
          int16_t *overlay_entry =
            (int16_t *)tag_block_get_element(overlay_anim_tag, i, 0x14);
          if (*overlay_entry != -1) {
            int16_t region_idx = overlay_entry[1];
            if ((int)region_idx < *(int *)((char *)object_tag + 0x158)) {
              void *region_block = tag_block_get_element(
                (char *)object_tag + 0x158, (int)region_idx, 0x168);
              void *overlay_anim_entry = tag_block_get_element(
                (char *)overlay_anim_tag + 0x74, (int)*overlay_entry, 0xb4);
              int16_t mode = overlay_entry[2];
              float func_value =
                *(float *)((char *)obj + 0xe4 + region_idx * 4);

              if (mode == 0) {
                /* Keyframe overlay */
                float total_frames;
                float frame_value;
                if ((*(uint8_t *)region_block & 2) == 0) {
                  total_frames =
                    (float)(int)(*(int16_t *)((char *)overlay_anim_entry +
                                              0x22) -
                                 1);
                } else {
                  total_frames =
                    (float)(int)*(int16_t *)((char *)overlay_anim_entry + 0x22);
                }
                frame_value = total_frames * func_value;
                ((animation_overlay_keyframe_fn)0x122690)(
                  overlay_anim_entry, frame_value, anim_data);
              } else if (mode == 1) {
                /* Interpolated overlay */
                int time = game_time_get();
                uint32_t frame_mod =
                  (uint32_t)(time + object_handle) %
                  (uint32_t)(int)*(int16_t *)((char *)overlay_anim_entry +
                                              0x22);
                ((animation_overlay_interpolate_fn)0x122450)(
                  overlay_anim_entry, (int)frame_mod, anim_data, anim_data);
              }
            }
          }
          overlay_idx = (int16_t)(overlay_idx + 1);
          i = (int)overlay_idx;
        } while (i < overlay_count);
      }
    }

    /* Scale animation data if object has a non-zero scale factor */
    if (*(float *)((char *)obj + 0x60) > *(float *)0x2533c0) {
      float scale = *(float *)((char *)obj + 0x60);
      *(float *)((char *)anim_data + 0x1c) =
        scale * *(float *)((char *)anim_data + 0x1c);
      *(float *)((char *)anim_data + 0x10) =
        scale * *(float *)((char *)anim_data + 0x10);
      *(float *)((char *)anim_data + 0x14) =
        scale * *(float *)((char *)anim_data + 0x14);
      *(float *)((char *)anim_data + 0x18) =
        scale * *(float *)((char *)anim_data + 0x18);
    }

    /* Call type-specific overlay adjustments */
    if (*(int *)((char *)object_tag + 0x44) != -1) {
      ((overlay_adjust_fn)0x13c7a0)(object_handle, anim_data);
    }

    /* Interpolate node matrices if the object has interpolation data */
    if (*(int16_t *)((char *)obj + 0x86) > 0) {
      void *interp_data;
      if (cannot_interpolate) {
        display_assert("!TEST_FLAG(_object_mask_cannot_interpolate, "
                       "object->object.type)",
                       "c:\\halo\\SOURCE\\objects\\objects.c", 0xad9, 1);
        system_exit(-1);
      }
      interp_data = object_header_block_reference_get(
        object_handle, (void *)((char *)obj + 0x198));
      ((anim_interpolate_fn)0x120ba0)(
        *(int16_t *)((char *)model_tag + 0xb8), interp_data, anim_data,
        *(int16_t *)((char *)obj + 0x84), *(int16_t *)((char *)obj + 0x86));
    }

    /* Validate object position and orientation if not using override */
    if (!override_decompressor) {
      if (!((valid_real_point3d_fn)0xa16b0)((float *)((char *)obj + 0x0c))) {
        char *name = (char *)tag_get_name(*(int *)obj);
        char *msg =
          csprintf((char *)0x5ab100,
                   "%s had a bad position before compute_node_matrices "
                   "(%f,%f,%f)",
                   name, (double)*(float *)((char *)obj + 0x0c),
                   (double)*(float *)((char *)obj + 0x10),
                   (double)*(float *)((char *)obj + 0x14));
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xae2, 1);
        system_exit(-1);
      }
      if (!((valid_fwd_and_up_fn)0x84a70)((float *)((char *)obj + 0x24),
                                          (float *)((char *)obj + 0x30))) {
        char *name = (char *)tag_get_name(*(int *)obj);
        char *msg = csprintf((char *)0x5ab100,
                             "%s had a bad forward and up before "
                             "compute_node_matrices (%f,%f,%f)x(%f,%f,%f)",
                             name, (double)*(float *)((char *)obj + 0x24),
                             (double)*(float *)((char *)obj + 0x28),
                             (double)*(float *)((char *)obj + 0x2c),
                             (double)*(float *)((char *)obj + 0x30),
                             (double)*(float *)((char *)obj + 0x34),
                             (double)*(float *)((char *)obj + 0x38));
        display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xae3, 1);
        system_exit(-1);
      }
    }

    /* Walk the node hierarchy via breadth-first traversal */
    {
      uint16_t node_queue[64];
      int queue_read = 0;
      int queue_write = 1;
      void *model_nodes_block = (char *)model_tag + 0xb8;

      float root_anim[13];
      float orientation_matrix[13];
      float translation_matrix[13];
      float phys_offset_matrix[13];
      float origin_matrix[13];
      float parent_copy[13];

      node_queue[0] = 0;

      do {
        int16_t cur_read = (int16_t)queue_read;
        uint16_t node_idx_u16 = node_queue[cur_read];
        int node_idx;
        void *node_data;
        queue_read++;
        node_idx = (int)(int16_t)node_idx_u16;
        node_data = tag_block_get_element(model_nodes_block, node_idx, 0x9c);

        if ((int16_t)node_idx_u16 == 0) {
          /* Root node processing */
          ((model_node_set_default_fn)0x109500)(root_anim, anim_data);

          if (!override_decompressor) {
            /* Build orientation matrix from object's forward and up */
            ((matrix_4x3_from_point_fn)0x109280)(translation_matrix,
                                                 (float *)((char *)obj + 0x0c));
            ((void (*)(float *, float *, float *))0x109e10)(
              orientation_matrix, (float *)((char *)obj + 0x24),
              (float *)((char *)obj + 0x30));

            /* Negate left column if object flag 0x1000 is set */
            if ((*(uint32_t *)((char *)obj + 0x4) & 0x1000) != 0) {
              orientation_matrix[4] = -orientation_matrix[4];
              orientation_matrix[5] = -orientation_matrix[5];
              orientation_matrix[6] = -orientation_matrix[6];
            }

            /* Apply physics center-of-mass offset if present */
            if (*(int *)((char *)object_tag + 0x8c) != -1) {
              void *phys_tag =
                tag_get(0x70687973, *(int *)((char *)object_tag + 0x8c));
              float neg_com[3];
              neg_com[0] = -*(float *)((char *)phys_tag + 0x0c);
              neg_com[1] = -*(float *)((char *)phys_tag + 0x10);
              neg_com[2] = -*(float *)((char *)phys_tag + 0x14);
              ((matrix_4x3_from_point_fn)0x109280)(phys_offset_matrix, neg_com);
              ((matrix_4x3_multiply_fn)0x109850)(
                orientation_matrix, phys_offset_matrix, orientation_matrix);
            }

            /* Apply model origin offset */
            ((matrix_4x3_from_point_fn)0x109280)(
              origin_matrix, (float *)((char *)object_tag + 0x14));
            ((matrix_4x3_multiply_fn)0x109850)(
              orientation_matrix, origin_matrix, orientation_matrix);

            if (parent_node_mat == NULL) {
              /* No parent — compose directly */
              ((matrix_4x3_multiply_fn)0x109850)(
                translation_matrix, orientation_matrix, node_matrices);
              ((matrix_4x3_multiply_fn)0x109850)(node_matrices, root_anim,
                                                 node_matrices);
            } else {
              /* Has parent — may need to scale and adjust */
              if (*(uint32_t *)parent_node_mat != 0x3f800000) {
                /* Parent scale != 1.0: scale the orientation position */
                float pscale = *parent_node_mat;
                int k;
                float *src;
                float *dst;
                orientation_matrix[10] *= pscale;
                orientation_matrix[11] *= pscale;
                orientation_matrix[12] *= pscale;
                /* Copy parent matrix to local buffer and set scale=1 */
                src = parent_node_mat;
                dst = parent_copy;
                for (k = 0xd; k != 0; k--) {
                  *dst = *src;
                  src++;
                  dst++;
                }
                parent_node_mat = parent_copy;
                parent_copy[0] = 1.0f;
                obj = (object_data_t *)object_get_and_verify_type(
                  object_handle, -1); /* decompiler artifact: reload ESI */
              }

              /* Check if parent object has flag 0x1000 (mirrored) */
              {
                void *parent_obj = object_get_and_verify_type(
                  obj->parent_object_index.value, -1);
                if ((*(uint32_t *)((char *)parent_obj + 0x4) & 0x1000) != 0) {
                  /* Copy parent matrix if not already copied */
                  if (parent_node_mat != parent_copy) {
                    float *src2 = parent_node_mat;
                    float *dst2 = parent_copy;
                    int k2;
                    for (k2 = 0xd; k2 != 0; k2--) {
                      *dst2 = *src2;
                      src2++;
                      dst2++;
                    }
                    parent_node_mat = parent_copy;
                    obj = (object_data_t *)object_get_and_verify_type(
                      object_handle, -1);
                  }
                  /* Negate the left column of parent matrix */
                  parent_node_mat[4] = -parent_node_mat[4];
                  parent_node_mat[5] = -parent_node_mat[5];
                  parent_node_mat[6] = -parent_node_mat[6];
                }
              }

              /* Validate parent node matrix */
              {
                uint32_t scale_bits = *(uint32_t *)parent_node_mat & 0x7f800000;
                if (scale_bits == 0x7f800000 ||
                    !((valid_real_vectors_fn)0xf6c40)(parent_node_mat + 1,
                                                      parent_node_mat + 4,
                                                      parent_node_mat + 7) ||
                    !((valid_real_point3d_fn)0xa16b0)(parent_node_mat + 10)) {
                  /* Parent node matrix is invalid — detailed error
                   * reporting */
                  void *parent_obj_2 = object_get_and_verify_type(
                    obj->parent_object_index.value, -1);
                  char *obj_name = (char *)tag_get_name(*(int *)obj);
                  char *parent_name =
                    (char *)tag_get_name(*(int *)parent_obj_2);
                  char *context =
                    csprintf((char *)0x5ab100, "%s as parent node of %s",
                             parent_name, obj_name);

                  /* assert_valid_real_matrix4x3 expanded inline */
                  if ((*(uint32_t *)parent_node_mat & 0x7f800000) ==
                      0x7f800000) {
                    char *msg =
                      csprintf((char *)0x5ab100, "%s had a bad scale %f",
                               context, (double)*parent_node_mat);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                  if (!valid_real_normal3d(parent_node_mat + 1)) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s had a bad forward (%f,%f,%f)",
                      context, (double)parent_node_mat[1],
                      (double)parent_node_mat[2], (double)parent_node_mat[3]);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                  if (!valid_real_normal3d(parent_node_mat + 4)) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s had a bad left (%f,%f,%f)", context,
                      (double)parent_node_mat[4], (double)parent_node_mat[5],
                      (double)parent_node_mat[6]);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                  if (!valid_real_normal3d(parent_node_mat + 7)) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s had a bad up (%f,%f,%f)", context,
                      (double)parent_node_mat[7], (double)parent_node_mat[8],
                      (double)parent_node_mat[9]);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                  if (!((valid_real_point3d_fn)0xa16b0)(parent_node_mat + 10)) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s had a bad position (%f,%f,%f)",
                      context, (double)parent_node_mat[10],
                      (double)parent_node_mat[11], (double)parent_node_mat[12]);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                  {
                    float dot_fl = parent_node_mat[1] * parent_node_mat[4] +
                                   parent_node_mat[2] * parent_node_mat[5] +
                                   parent_node_mat[3] * parent_node_mat[6];
                    if ((*(uint32_t *)&dot_fl & 0x7f800000) == 0x7f800000 ||
                        (dot_fl < 0 ? -dot_fl : dot_fl) >= *(float *)0x2549d8) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a forward (%f,%f,%f) not perpendicular "
                        "to left (%f,%f,%f)",
                        context, (double)parent_node_mat[1],
                        (double)parent_node_mat[2], (double)parent_node_mat[3],
                        (double)parent_node_mat[4], (double)parent_node_mat[5],
                        (double)parent_node_mat[6]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb37, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float dot_ul = parent_node_mat[7] * parent_node_mat[4] +
                                   parent_node_mat[8] * parent_node_mat[5] +
                                   parent_node_mat[9] * parent_node_mat[6];
                    if ((*(uint32_t *)&dot_ul & 0x7f800000) == 0x7f800000 ||
                        (dot_ul < 0 ? -dot_ul : dot_ul) >= *(float *)0x2549d8) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a up (%f,%f,%f) not perpendicular to "
                        "left (%f,%f,%f)",
                        context, (double)parent_node_mat[7],
                        (double)parent_node_mat[8], (double)parent_node_mat[9],
                        (double)parent_node_mat[4], (double)parent_node_mat[5],
                        (double)parent_node_mat[6]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb37, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float dot_uf = parent_node_mat[7] * parent_node_mat[1] +
                                   parent_node_mat[2] * parent_node_mat[8] +
                                   parent_node_mat[3] * parent_node_mat[9];
                    if ((*(uint32_t *)&dot_uf & 0x7f800000) == 0x7f800000 ||
                        (dot_uf < 0 ? -dot_uf : dot_uf) >= *(float *)0x2549d8) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a forward (%f,%f,%f) not perpendicular "
                        "to up (%f,%f,%f)",
                        context, (double)parent_node_mat[1],
                        (double)parent_node_mat[2], (double)parent_node_mat[3],
                        (double)parent_node_mat[7], (double)parent_node_mat[8],
                        (double)parent_node_mat[9]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb37, 1);
                      system_exit(-1);
                    }
                  }
                  if (!((valid_real_matrix4x3_fn)0xf6d00)(parent_node_mat)) {
                    char *msg =
                      csprintf((char *)0x5ab100,
                               "%s: assert_valid_real_matrix4x3", context);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
                    system_exit(-1);
                  }
                }
              }

              /* Compose parent * translation, then node * orientation,
               * then multiply with root anim */
              ((matrix_4x3_multiply_fn)0x109850)(
                parent_node_mat, translation_matrix, node_matrices);
              ((matrix_4x3_multiply_fn)0x109850)(
                node_matrices, orientation_matrix, node_matrices);
              ((matrix_4x3_multiply_fn)0x109850)(node_matrices, root_anim,
                                                 node_matrices);
            }
          } else {
            /* override_decompressor — just copy root_anim to node_matrices */
            float *src3 = root_anim;
            float *dst3 = node_matrices;
            int k3;
            for (k3 = 0xd; k3 != 0; k3--) {
              *dst3 = *src3;
              src3++;
              dst3++;
            }
          }

          /* Validate root node matrix — first quick check */
          {
            float *fwd = node_matrices + 1;
            float *left = node_matrices + 4;
            uint32_t scale_bits = *(uint32_t *)node_matrices & 0x7f800000;
            if (scale_bits == 0x7f800000 ||
                !((valid_real_vectors_fn)0xf6c40)(fwd, left,
                                                  node_matrices + 7) ||
                (*(uint32_t *)&node_matrices[10] & 0x7f800000) == 0x7f800000 ||
                (*(uint32_t *)&node_matrices[11] & 0x7f800000) == 0x7f800000 ||
                (*(uint32_t *)&node_matrices[12] & 0x7f800000) == 0x7f800000) {
              {
                /* Root node matrix invalid — dump diagnostic info */
                char *name;
                obj = (object_data_t *)object_get_and_verify_type(object_handle,
                                                                  -1);
                name = (char *)tag_get_name(*(int *)obj);
                error(2,
                      "object_compute_node_matrices FAILURE on root node "
                      "of %s",
                      name);
                error(2, "  object: pos %f %f %f, fwd %f %f %f, up %f %f %f",
                      (double)*(float *)((char *)obj + 0x0c),
                      (double)*(float *)((char *)obj + 0x10),
                      (double)*(float *)((char *)obj + 0x14),
                      (double)*(float *)((char *)obj + 0x24),
                      (double)*(float *)((char *)obj + 0x28),
                      (double)*(float *)((char *)obj + 0x2c),
                      (double)*(float *)((char *)obj + 0x30),
                      (double)*(float *)((char *)obj + 0x34),
                      (double)*(float *)((char *)obj + 0x38));

                if (*(int *)((char *)object_tag + 0x8c) != -1) {
                  void *phys_tag2 =
                    tag_get(0x70687973, *(int *)((char *)object_tag + 0x8c));
                  error(2, "  center-of-mass translation %f %f %f",
                        (double)-*(float *)((char *)phys_tag2 + 0x0c),
                        (double)-*(float *)((char *)phys_tag2 + 0x10),
                        (double)-*(float *)((char *)phys_tag2 + 0x14));
                }
                error(2, "  origin-offset %f %f %f",
                      (double)*(float *)((char *)object_tag + 0x14),
                      (double)*(float *)((char *)object_tag + 0x18),
                      (double)*(float *)((char *)object_tag + 0x1c));

                if (parent_node_mat == NULL) {
                  error(2, "  no parent node");
                } else {
                  error(2, "  parent-node matrix fwd  %f %f %f",
                        (double)parent_node_mat[1], (double)parent_node_mat[2],
                        (double)parent_node_mat[3]);
                  error(2, "                     left %f %f %f",
                        (double)parent_node_mat[4], (double)parent_node_mat[5],
                        (double)parent_node_mat[6]);
                  error(2, "                     up   %f %f %f",
                        (double)parent_node_mat[7], (double)parent_node_mat[8],
                        (double)parent_node_mat[9]);
                  error(2, "                     posn %f %f %f",
                        (double)parent_node_mat[10],
                        (double)parent_node_mat[11],
                        (double)parent_node_mat[12]);
                  error(2,
                        "                     scale (jason's ugly secret) "
                        "%f",
                        (double)*parent_node_mat);
                }

                error(2, "");

                error(2, "computed matrix fwd  %f %f %f",
                      (double)node_matrices[1], (double)node_matrices[2],
                      (double)node_matrices[3]);
                error(2, "                left %f %f %f",
                      (double)node_matrices[4], (double)node_matrices[5],
                      (double)node_matrices[6]);
                error(2, "                up   %f %f %f",
                      (double)node_matrices[7], (double)node_matrices[8],
                      (double)node_matrices[9]);
                error(2, "                posn %f %f %f",
                      (double)node_matrices[10], (double)node_matrices[11],
                      (double)node_matrices[12]);
                error(2, "                scale %f", (double)*node_matrices);

                /* assert_valid_real_matrix4x3 on root node (line 0xb69)
                 */
                if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000 ||
                    !((valid_real_vectors_fn)0xf6c40)(fwd, left,
                                                      node_matrices + 7) ||
                    !((valid_real_point3d_fn)0xa16b0)(node_matrices + 10)) {
                  if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000) {
                    char *msg =
                      csprintf((char *)0x5ab100, "%s had a bad scale %f",
                               "object_compute_node_matrices root node matrix",
                               (double)*node_matrices);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb69, 1);
                    system_exit(-1);
                  }
                  {
                    float mag_fwd = fwd[0] * fwd[0] + fwd[1] * fwd[1] +
                                    fwd[2] * fwd[2] - *(float *)0x2533c8;
                    if ((*(uint32_t *)&mag_fwd & 0x7f800000) == 0x7f800000 ||
                        (mag_fwd < 0 ? -mag_fwd : mag_fwd) >=
                          *(float *)0x2549d8) {
                      char *msg = csprintf(
                        (char *)0x5ab100, "%s had a bad forward (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)fwd[0], (double)fwd[1], (double)fwd[2]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float mag_left = left[0] * left[0] + left[1] * left[1] +
                                     left[2] * left[2] - *(float *)0x2533c8;
                    if ((*(uint32_t *)&mag_left & 0x7f800000) == 0x7f800000 ||
                        (mag_left < 0 ? -mag_left : mag_left) >=
                          *(float *)0x2549d8) {
                      char *msg = csprintf(
                        (char *)0x5ab100, "%s had a bad left (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)left[0], (double)left[1], (double)left[2]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float mag_up = node_matrices[9] * node_matrices[9] +
                                   node_matrices[8] * node_matrices[8] +
                                   node_matrices[7] * node_matrices[7] -
                                   *(float *)0x2533c8;
                    if ((*(uint32_t *)&mag_up & 0x7f800000) == 0x7f800000 ||
                        (mag_up < 0 ? -mag_up : mag_up) >= *(float *)0x2549d8) {
                      char *msg = csprintf(
                        (char *)0x5ab100, "%s had a bad up (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)node_matrices[7], (double)node_matrices[8],
                        (double)node_matrices[9]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  if ((*(uint32_t *)&node_matrices[10] & 0x7f800000) ==
                        0x7f800000 ||
                      (*(uint32_t *)&node_matrices[11] & 0x7f800000) ==
                        0x7f800000 ||
                      (*(uint32_t *)&node_matrices[12] & 0x7f800000) ==
                        0x7f800000) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s had a bad position (%f,%f,%f)",
                      "object_compute_node_matrices root node matrix",
                      (double)node_matrices[10], (double)node_matrices[11],
                      (double)node_matrices[12]);
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb69, 1);
                    system_exit(-1);
                  }
                  {
                    float dot_fl =
                      fwd[0] * left[0] + fwd[1] * left[1] + fwd[2] * left[2];
                    if ((*(uint32_t *)&dot_fl & 0x7f800000) == 0x7f800000 ||
                        (dot_fl < 0 ? -dot_fl : dot_fl) >= *(float *)0x2549d8) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a forward (%f,%f,%f) not perpendicular "
                        "to left (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)fwd[0], (double)fwd[1], (double)fwd[2],
                        (double)left[0], (double)left[1], (double)left[2]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float dot_ul = left[0] * node_matrices[7] +
                                   node_matrices[8] * left[1] +
                                   node_matrices[9] * left[2];
                    if ((*(uint32_t *)&dot_ul & 0x7f800000) == 0x7f800000 ||
                        (dot_ul < 0 ? -dot_ul : dot_ul) >= *(float *)0x2549d8) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a up (%f,%f,%f) not perpendicular to "
                        "left (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)node_matrices[7], (double)node_matrices[8],
                        (double)node_matrices[9], (double)left[0],
                        (double)left[1], (double)left[2]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  {
                    float dot_uf = node_matrices[7] * fwd[0] +
                                   node_matrices[8] * fwd[1] +
                                   node_matrices[9] * fwd[2];
                    if ((*(uint32_t *)&dot_uf & 0x7f800000) == 0x7f800000 ||
                        (dot_uf < 0 ? -dot_uf : dot_uf) >= *(float *)0x2549d8) {
                      char *msg = csprintf(
                        (char *)0x5ab100,
                        "%s had a forward (%f,%f,%f) not perpendicular "
                        "to up (%f,%f,%f)",
                        "object_compute_node_matrices root node matrix",
                        (double)fwd[0], (double)fwd[1], (double)fwd[2],
                        (double)node_matrices[7], (double)node_matrices[8],
                        (double)node_matrices[9]);
                      display_assert(
                        msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb69, 1);
                      system_exit(-1);
                    }
                  }
                  if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000 ||
                      !((valid_real_vectors_fn)0xf6c40)(fwd, left,
                                                        node_matrices + 7) ||
                      !((valid_real_point3d_fn)0xa16b0)(node_matrices + 10)) {
                    char *msg = csprintf(
                      (char *)0x5ab100, "%s: assert_valid_real_matrix4x3",
                      "object_compute_node_matrices root node matrix");
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb69, 1);
                    system_exit(-1);
                  }
                }
              }
            }
          }

          /* Final assert_valid_real_matrix4x3 on root (line 0xb77) */
          {
            float *fwd2 = node_matrices + 1;
            float *left2 = node_matrices + 4;
            if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000 ||
                !((valid_real_vectors_fn)0xf6c40)(fwd2, left2,
                                                  node_matrices + 7) ||
                (*(uint32_t *)&node_matrices[10] & 0x7f800000) == 0x7f800000 ||
                (*(uint32_t *)&node_matrices[11] & 0x7f800000) == 0x7f800000 ||
                (*(uint32_t *)&node_matrices[12] & 0x7f800000) == 0x7f800000) {
              char *name2 = (char *)tag_get_name(*(int *)obj);

              if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000) {
                char *msg = csprintf((char *)0x5ab100, "%s had a bad scale %f",
                                     name2, (double)*node_matrices);
                display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                               0xb77, 1);
                system_exit(-1);
              }
              {
                float mag_fwd = fwd2[0] * fwd2[0] + fwd2[1] * fwd2[1] +
                                fwd2[2] * fwd2[2] - *(float *)0x2533c8;
                if ((*(uint32_t *)&mag_fwd & 0x7f800000) == 0x7f800000 ||
                    (mag_fwd < 0 ? -mag_fwd : mag_fwd) >= *(float *)0x2549d8) {
                  char *msg = csprintf(
                    (char *)0x5ab100, "%s had a bad forward (%f,%f,%f)", name2,
                    (double)fwd2[0], (double)fwd2[1], (double)fwd2[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              {
                float mag_left = left2[0] * left2[0] + left2[1] * left2[1] +
                                 left2[2] * left2[2] - *(float *)0x2533c8;
                if ((*(uint32_t *)&mag_left & 0x7f800000) == 0x7f800000 ||
                    (mag_left < 0 ? -mag_left : mag_left) >=
                      *(float *)0x2549d8) {
                  char *msg = csprintf(
                    (char *)0x5ab100, "%s had a bad left (%f,%f,%f)", name2,
                    (double)left2[0], (double)left2[1], (double)left2[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              {
                float mag_up = node_matrices[9] * node_matrices[9] +
                               node_matrices[8] * node_matrices[8] +
                               node_matrices[7] * node_matrices[7] -
                               *(float *)0x2533c8;
                if ((*(uint32_t *)&mag_up & 0x7f800000) == 0x7f800000 ||
                    (mag_up < 0 ? -mag_up : mag_up) >= *(float *)0x2549d8) {
                  char *msg = csprintf(
                    (char *)0x5ab100, "%s had a bad up (%f,%f,%f)", name2,
                    (double)node_matrices[7], (double)node_matrices[8],
                    (double)node_matrices[9]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              if ((*(uint32_t *)&node_matrices[10] & 0x7f800000) ==
                    0x7f800000 ||
                  (*(uint32_t *)&node_matrices[11] & 0x7f800000) ==
                    0x7f800000 ||
                  (*(uint32_t *)&node_matrices[12] & 0x7f800000) ==
                    0x7f800000) {
                char *msg = csprintf(
                  (char *)0x5ab100, "%s had a bad position (%f,%f,%f)", name2,
                  (double)node_matrices[10], (double)node_matrices[11],
                  (double)node_matrices[12]);
                display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                               0xb77, 1);
                system_exit(-1);
              }
              {
                float dot_fl =
                  fwd2[0] * left2[0] + fwd2[1] * left2[1] + fwd2[2] * left2[2];
                if ((*(uint32_t *)&dot_fl & 0x7f800000) == 0x7f800000 ||
                    (dot_fl < 0 ? -dot_fl : dot_fl) >= *(float *)0x2549d8) {
                  char *msg = csprintf(
                    (char *)0x5ab100,
                    "%s had a forward (%f,%f,%f) not perpendicular "
                    "to left (%f,%f,%f)",
                    name2, (double)fwd2[0], (double)fwd2[1], (double)fwd2[2],
                    (double)left2[0], (double)left2[1], (double)left2[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              {
                float dot_ul = left2[0] * node_matrices[7] +
                               node_matrices[8] * left2[1] +
                               node_matrices[9] * left2[2];
                if ((*(uint32_t *)&dot_ul & 0x7f800000) == 0x7f800000 ||
                    (dot_ul < 0 ? -dot_ul : dot_ul) >= *(float *)0x2549d8) {
                  char *msg = csprintf(
                    (char *)0x5ab100,
                    "%s had a up (%f,%f,%f) not perpendicular to "
                    "left (%f,%f,%f)",
                    name2, (double)node_matrices[7], (double)node_matrices[8],
                    (double)node_matrices[9], (double)left2[0],
                    (double)left2[1], (double)left2[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              {
                float dot_uf = node_matrices[7] * fwd2[0] +
                               fwd2[1] * node_matrices[8] +
                               fwd2[2] * node_matrices[9];
                if ((*(uint32_t *)&dot_uf & 0x7f800000) == 0x7f800000 ||
                    (dot_uf < 0 ? -dot_uf : dot_uf) >= *(float *)0x2549d8) {
                  char *msg = csprintf(
                    (char *)0x5ab100,
                    "%s had a forward (%f,%f,%f) not perpendicular "
                    "to up (%f,%f,%f)",
                    name2, (double)fwd2[0], (double)fwd2[1], (double)fwd2[2],
                    (double)node_matrices[7], (double)node_matrices[8],
                    (double)node_matrices[9]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb77, 1);
                  system_exit(-1);
                }
              }
              if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000 ||
                  !((valid_real_vectors_fn)0xf6c40)(fwd2, left2,
                                                    node_matrices + 7) ||
                  !((valid_real_point3d_fn)0xa16b0)(node_matrices + 10)) {
                char *msg = csprintf((char *)0x5ab100,
                                     "%s: assert_valid_real_matrix4x3", name2);
                display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                               0xb77, 1);
                system_exit(-1);
              }
            }
          }
        } else {
          /* Non-root node: set default pose then multiply by parent */
          float *node_mat = node_matrices + node_idx * 13;
          int16_t parent_idx;
          float *parent_mat;
          ((model_node_set_default_fn)0x109500)(node_mat, (char *)anim_data +
                                                            node_idx * 0x20);

          if (*(int16_t *)((char *)node_data + 0x24) == -1) {
            display_assert("node->parent_node_index!=NONE",
                           "c:\\halo\\SOURCE\\objects\\objects.c", 0xb71, 1);
            system_exit(-1);
          }
          parent_idx = *(int16_t *)((char *)node_data + 0x24);
          parent_mat = node_matrices + parent_idx * 13;
          ((matrix_4x3_multiply_fn)0x109850)(parent_mat, node_mat, node_mat);
        }

        /* Enqueue child and sibling nodes */
        if (*(uint16_t *)((char *)node_data + 0x20) != 0xffff) {
          node_queue[(int16_t)queue_write] =
            *(uint16_t *)((char *)node_data + 0x20);
          queue_write++;
        }
        if (*(uint16_t *)((char *)node_data + 0x22) != 0xffff) {
          node_queue[(int16_t)queue_write] =
            *(uint16_t *)((char *)node_data + 0x22);
          queue_write++;
        }
      } while ((int16_t)queue_read != (int16_t)queue_write);
    }
  }

  /* Apply origin offset from model tag and set bounding sphere radius */
  matrix_transform_point(node_matrices, (float *)((char *)object_tag + 0x08),
                         (float *)((char *)obj + 0x50));
  {
    float radius = *(float *)((char *)object_tag + 0x04);
    *(float *)((char *)obj + 0x5c) = radius;
    if (*(float *)((char *)obj + 0x60) > *(float *)0x2533c0) {
      *(float *)((char *)obj + 0x5c) = radius * *(float *)((char *)obj + 0x60);
    }
  }
}

/* objects_scripting_detach — scripting wrapper: detach param_2 from param_1.
 * Checks that param_2's parent_object_index (+0xcc) matches param_1, then
 * calls object_detach_from_parent on param_2.
 * 0x143510 / objects.obj
 */
void objects_scripting_detach(int param_1, int param_2)
{
  int iVar1;

  if (param_1 != -1 && param_2 != -1) {
    iVar1 = (int)object_get_and_verify_type(param_2, 0xffffffff);
    if (*(int *)(iVar1 + 0xcc) == param_1) {
      object_detach_from_parent(param_2);
    }
  }
}

/* attachments_delete — delete object attachments (effects, sounds, lights,
 * etc.).
 *
 * Iterates through the object's attachment slots (up to tag+0x140 count)
 * and dispatches cleanup calls based on attachment type:
 *   Type 0: light_delete (effect cleanup)
 *   Type 1: game_looping_sound_delete (sound cleanup)
 *   Type 2: effect_delete (decal cleanup)
 *   Type 3: FUN_00141b70 + FUN_000986d0 (light cleanup)
 *   Type 4: FUN_0009f6e0 (contrail cleanup)
 *
 * Object attachment structure:
 *   obj+0xf4 to obj+0xf4+count: attachment type bytes (-1 = empty)
 *   obj+0xfc + index*4: attachment handle (int)
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0x1ba140 (tag_get) with ('obje', obj[0]).
 * Confirmed: switch jump table at 0x143ac0 for 5 cases.
 */
void attachments_delete(int object_handle)
{
  int *obj;
  char *tag;
  int16_t i;
  char type;
  int attachment_handle;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  tag = (char *)tag_get(0x6f626a65, obj[0]);

  for (i = 0; i < *(int *)(tag + 0x140); i++) {
    type = *((char *)obj + 0xf4 + (int)i);
    if (type == -1)
      continue;

    attachment_handle = *(int *)((char *)obj + 0xfc + (int)i * 4);
    if (attachment_handle == -1)
      continue;

    switch (type) {
    case 0:
      light_delete(attachment_handle);
      break;
    case 1:
      game_looping_sound_delete(attachment_handle);
      break;
    case 2:
      effect_delete(attachment_handle);
      break;
    case 3:
      object_compute_node_matrices(object_handle);
      contrail_set_state_for_object(attachment_handle, 1, 0);
      break;
    case 4:
      FUN_0009f6e0(attachment_handle);
      break;
    }
  }
}

/* object_set_position — reposition an object's position and facing.
 *
 * Disconnects the object from the map, optionally updates its position
 * (forward vector at obj+0x0C) and facing direction (at obj+0x24).
 * If a target (up) vector is provided, it is copied directly to obj+0x30.
 * Otherwise, a perpendicular up vector is computed from the facing via:
 *   temp = {facing.y, -facing.x, 0.0}
 *   normalize(temp)
 *   if degenerate: temp = {1, 0, 0}
 *   up = cross(temp, facing)
 * Then recomputes node matrices and reconnects to the map.
 *
 * Confirmed: 4 cdecl args (object_handle, facing, target, flags).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0x13fd00 (object_disconnect_from_map) with 1 stack arg.
 * Confirmed: CALL 0x13010 (normalize3d) for perpendicular temp vector.
 * Confirmed: cross product computed via x87 FPU in-line (not a function call).
 * Confirmed: CALL 0x141b70 (object_compute_node_matrices).
 * Confirmed: CALL 0x140ce0 (object_connect_to_map) with (handle, 0).
 * Confirmed: FCOMP against *(float*)0x2533c0 (0.0f) for degenerate check.
 */
void object_set_position(int object_handle, float *position, float *forward,
                         float *up)
{
  char *obj;
  float temp[3];
  float mag;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  object_disconnect_from_map(object_handle);

  /* Copy position if provided */
  if (position != NULL) {
    *(float *)(obj + 0x0c) = position[0];
    *(float *)(obj + 0x10) = position[1];
    *(float *)(obj + 0x14) = position[2];
  }

  /* Copy forward direction and compute/set up vector */
  if (forward != NULL) {
    *(float *)(obj + 0x24) = forward[0];
    *(float *)(obj + 0x28) = forward[1];
    *(float *)(obj + 0x2c) = forward[2];

    if (up != NULL) {
      /* Up vector provided directly */
      *(float *)(obj + 0x30) = up[0];
      *(float *)(obj + 0x34) = up[1];
      *(float *)(obj + 0x38) = up[2];
    } else {
      /* Compute perpendicular up from forward direction:
       * temp = {forward.y, -forward.x, 0.0} */
      temp[0] = forward[1];
      temp[1] = -forward[0];
      temp[2] = 0.0f;

      mag = normalize3d(temp);
      if (mag == *(float *)0x2533c0) {
        /* Degenerate (forward is along Z) — use X axis */
        temp[0] = 1.0f;
        temp[2] = 0.0f;
        temp[1] = 0.0f;
      }

      /* up = cross(temp, forward) */
      *(float *)(obj + 0x30) = temp[1] * forward[2] - temp[2] * forward[1];
      *(float *)(obj + 0x34) = temp[2] * forward[0] - temp[0] * forward[2];
      *(float *)(obj + 0x38) = temp[0] * forward[1] - temp[1] * forward[0];
    }
  }

  object_compute_node_matrices(object_handle);
  object_connect_to_map(object_handle, 0);
}

/* object_translate — set an object's position and reconnect it to the map.
 *
 * Validates the new position with valid_real_point3d, asserts if invalid.
 * Disconnects the object from the BSP, copies the 3D position into the
 * object data at offset +0x0C, then reconnects with the given location.
 *
 * The assert string identifies this as "new_position" in objects.c line 0x232.
 *
 * Confirmed: 3 cdecl args (object_handle, position, location).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (handle, -1).
 * Confirmed: CALL 0xa16b0 (valid_real_point3d) for point validation.
 * Confirmed: CALL 0x8d9d0 (csprintf) for assert message formatting.
 * Confirmed: CALL 0x8d9f0 (display_assert) with file/line left on stack.
 * Confirmed: CALL 0x8e2f0 (system_exit) with -1.
 * Confirmed: CALL 0x13fd00 (object_disconnect_from_map).
 * Confirmed: CALL 0x140ce0 (object_connect_to_map) with (handle, location).
 * Confirmed: position copied to obj+0x0C, obj+0x10, obj+0x14.
 */
void object_translate(int object_handle, float *position, void *location)
{
  char *obj;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if (!valid_real_point3d(position)) {
    char *msg =
      csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
               "new_position", (double)position[0], (double)position[1],
               (double)position[2]);
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x232, 1);
    system_exit(-1);
  }
  object_disconnect_from_map(object_handle);
  *(float *)(obj + 0x0c) = position[0];
  *(float *)(obj + 0x10) = position[1];
  *(float *)(obj + 0x14) = position[2];
  object_connect_to_map(object_handle, location);
}

/*
 * object_new — create a new object from a placement data struct.
 *
 * This is the core object creation function. Validates the placement data,
 * allocates a datum in the object header table, initialises the object's
 * fields from the placement struct and the object definition tag, then runs
 * the full chain of type-specific initialisers, node matrix computation,
 * map connection, widget creation, and child attachment.
 *
 * On failure (no free slots or type init failure), the datum is freed and
 * an "OUT OF OBJECTS" error is logged. Returns -1 on failure, or the new
 * object's datum handle on success.
 *
 * Confirmed: SUB ESP,0x210 — 528 bytes of locals (includes 512-byte sprintf
 * buffer). Confirmed: tag_get(0x6f626a65, tag_index) for 'obje' tag. Confirmed:
 * object_header_new with EAX=-1 for datum allocation. Confirmed: datum_get +
 * object_get_and_verify_type for header/obj access. Confirmed: header->unk_2 |=
 * 0x44 sets active+type flags. Confirmed: position += scale * up_vector
 * (placement+0x24 multiplied through). Confirmed: tag_get(0x6d6f6465, ...) for
 * 'mode' model tag, node count at +0xb8. Confirmed:
 * object_header_block_allocate for block reference allocation (3 calls).
 * Confirmed: FUN_0009ec30 creation effect (8 args) if tag_data+0xac != -1.
 * Confirmed: return EBX (object_handle or -1).
 */
int object_new(void *placement)
{
  char *p = (char *)placement;
  int tag_index;
  char *tag_data;
  char *obj;
  char *header;
  int object_handle;
  uint32_t node_count;
  uint8_t saved_active_bit;
  uint8_t success;
  char local_buf[512];

  tag_index = *(int *)p;

  /* --- Validation: position --- */
  if (!((valid_real_point3d_fn)0xa16b0)((float *)(p + 0x18))) {
    char *msg =
      csprintf((char *)0x5ab100, "%s: assert_valid_real_point3d(%f, %f, %f)",
               "&data->position", (double)*(float *)(p + 0x18),
               (double)*(float *)(p + 0x1c), (double)*(float *)(p + 0x20));
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x26a, 1);
    system_exit(-1);
  }

  /* --- Validation: forward/up axes perpendicularity --- */
  if (!valid_real_normal3d_perpendicular((float *)(p + 0x34),
                                         (float *)(p + 0x40))) {
    char *msg = csprintf(
      (char *)0x5ab100,
      "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
      "&data->forward", "&data->up", (double)*(float *)(p + 0x34),
      (double)*(float *)(p + 0x38), (double)*(float *)(p + 0x3c),
      (double)*(float *)(p + 0x40), (double)*(float *)(p + 0x44),
      (double)*(float *)(p + 0x48));
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x26b, 1);
    system_exit(-1);
  }

  /* --- Validation: angular velocity --- */
  if (!real_vector3d_valid((float *)(p + 0x4c))) {
    char *msg =
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&data->angular_velocity", (double)*(float *)(p + 0x4c),
               (double)*(float *)(p + 0x50), (double)*(float *)(p + 0x54));
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x26c, 1);
    system_exit(-1);
  }

  /* --- Validation: translational velocity --- */
  if (!real_vector3d_valid((float *)(p + 0x28))) {
    char *msg =
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&data->translational_velocity", (double)*(float *)(p + 0x28),
               (double)*(float *)(p + 0x2c), (double)*(float *)(p + 0x30));
    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x26d, 1);
    system_exit(-1);
  }

  /* --- Game engine tag remapping --- */
  if (game_engine_running()) {
    if (tag_index == -1)
      return -1;
    tag_index = FUN_000ae0a0(tag_index);
  }

  if (tag_index == -1)
    return -1;

  /* --- Get object definition tag --- */
  tag_data = (char *)tag_get(0x6f626a65, tag_index);

  /* --- Get type definition and allocate datum --- */
  {
    uint16_t type_word = *(uint16_t *)tag_data;
    char *type_def = (char *)FUN_0013c100((int16_t)type_word);
    int16_t datum_size = *(int16_t *)(type_def + 0x8);

    object_handle = object_header_new(*(data_t **)0x5a8d50, datum_size, -1);
  }

  if (object_handle == -1)
    goto out_of_objects;

  /* --- Get header and object pointers --- */
  header = (char *)datum_get(*(data_t **)0x5a8d50, object_handle);
  obj = (char *)object_get_and_verify_type(object_handle, -1);

  /* Set header flags and type */
  *(uint8_t *)(header + 0x2) |= 0x44;
  *(uint8_t *)(header + 0x3) = *(uint8_t *)tag_data;

  /* Store tag index and type in object */
  *(int *)obj = tag_index;
  success = 1;
  *(int16_t *)(obj + 0x64) = *(int16_t *)tag_data;

  /* --- Type-specific init callback (FUN_0013c430) --- */
  FUN_0013c430(object_handle, placement);

  /* --- Copy fields from placement to object --- */
  /* position: placement+0x18 → obj+0x0c */
  *(float *)(obj + 0x0c) = *(float *)(p + 0x18);
  *(float *)(obj + 0x10) = *(float *)(p + 0x1c);
  *(float *)(obj + 0x14) = *(float *)(p + 0x20);

  /* forward: placement+0x34 → obj+0x24 */
  *(float *)(obj + 0x24) = *(float *)(p + 0x34);
  *(float *)(obj + 0x28) = *(float *)(p + 0x38);
  *(float *)(obj + 0x2c) = *(float *)(p + 0x3c);

  /* up: placement+0x40 → obj+0x30 */
  *(float *)(obj + 0x30) = *(float *)(p + 0x40);
  *(float *)(obj + 0x34) = *(float *)(p + 0x44);
  *(float *)(obj + 0x38) = *(float *)(p + 0x48);

  /* velocity: placement+0x28 → obj+0x18 */
  *(float *)(obj + 0x18) = *(float *)(p + 0x28);
  *(float *)(obj + 0x1c) = *(float *)(p + 0x2c);
  *(float *)(obj + 0x20) = *(float *)(p + 0x30);

  /* angular velocity: placement+0x4c → obj+0x3c */
  *(float *)(obj + 0x3c) = *(float *)(p + 0x4c);
  *(float *)(obj + 0x40) = *(float *)(p + 0x50);
  *(float *)(obj + 0x44) = *(float *)(p + 0x54);

  /* position += scale * up_vector (placement+0x24 is the scale factor) */
  {
    float scale = *(float *)(p + 0x24);
    *(float *)(obj + 0x0c) += scale * *(float *)(obj + 0x30);
    *(float *)(obj + 0x10) += scale * *(float *)(obj + 0x34);
    *(float *)(obj + 0x14) += scale * *(float *)(obj + 0x38);
  }

  /* --- Set flag bit 12 based on placement flags bit 0 --- */
  {
    uint32_t flags = *(uint32_t *)(obj + 0x4);
    if (*(uint8_t *)(p + 0x4) & 0x1)
      flags |= 0x1000;
    else
      flags &= ~(uint32_t)0x1000;
    *(uint32_t *)(obj + 0x4) = flags;
  }

  /* --- Initialise various fields to defaults --- */
  *(int16_t *)(obj + 0x4c) = -1;
  *(int16_t *)(header + 0x4) = -1;
  *(uint32_t *)(obj + 0x8) = *(uint32_t *)0x5a8d28 - 1;
  *(int *)(obj + 0xa0) = -1;
  *(int *)(obj + 0xbc) = -1;
  *(int16_t *)(obj + 0x80) = -1;
  *(int *)(obj + 0x7c) = *(int *)(tag_data + 0x44);
  *(int *)(obj + 0x120) = -1;
  *(int *)(obj + 0xcc) = -1;
  *(int *)(obj + 0xc4) = -1;
  *(int *)(obj + 0xc8) = -1;
  *(int16_t *)(obj + 0x6a) = -1;
  *(int *)(obj + 0xac) = -1;
  *(int *)(obj + 0xb0) = -1;

  /* --- Tag flag propagation --- */
  if (*(uint8_t *)(tag_data + 0x2) & 0x1)
    *(uint32_t *)(obj + 0x4) |= 0x40000;

  {
    uint32_t oflags = *(uint32_t *)(obj + 0x4);
    if (*(int *)(tag_data + 0x7c) != -1)
      oflags |= 0x2000000;
    else
      oflags &= ~(uint32_t)0x2000000;
    *(uint32_t *)(obj + 0x4) = oflags;
  }

  /* --- Set garbage flag (1 if model tag exists, 0 if not) --- */
  object_set_garbage(object_handle, (uint8_t)(*(int *)(tag_data + 0x34) != -1));

  /* --- Copy remaining placement fields --- */
  *(int16_t *)(obj + 0x68) = *(int16_t *)(p + 0x14);
  *(int *)(obj + 0x70) = *(int *)(p + 0x08);
  *(int *)(obj + 0x74) = *(int *)(p + 0x0c);
  *(int16_t *)(obj + 0x6e) = *(int16_t *)(p + 0x16);
  *(int16_t *)(obj + 0x126) = *(int16_t *)(tag_data + 0x13e);

  /* --- Get node count from model tag --- */
  if (*(int *)(tag_data + 0x34) == -1) {
    node_count = 1;
  } else {
    char *model_tag = (char *)tag_get(0x6d6f6465, *(int *)(tag_data + 0x34));
    node_count = *(uint16_t *)(model_tag + 0xb8);
  }

  /* --- Allocate block references for node matrices --- */
  if (!object_header_block_allocate(object_handle, 0x1a0, node_count * 0x34)) {
    success = 0;
  } else if (((1 << (*(uint8_t *)tag_data & 0x1f)) & 0xfe0) == 0) {
    /* Non-standard types need additional allocations */
    if (!object_header_block_allocate(object_handle, 0x19c, node_count << 5) ||
        !object_header_block_allocate(object_handle, 0x198, node_count << 5)) {
      success = 0;
    }
  }

  /* --- Re-acquire object pointer (may have moved due to allocation) --- */
  obj = (char *)object_get_and_verify_type(object_handle, -1);

  if (success && FUN_0013c490(object_handle)) {
    /* --- Save and optionally clear the active (bit 19) flag --- */
    saved_active_bit = (uint8_t)((*(uint32_t *)(obj + 0x4) >> 19) & 1);
    if (saved_active_bit && (*(uint8_t *)(p + 0x4) & 0x2)) {
      *(uint32_t *)(obj + 0x4) &= ~(uint32_t)0x80000;
    }

    /* --- Run initialisation chain --- */
    object_choose_random_change_colors(object_handle, p + 0x58);
    object_choose_random_region_permutations(object_handle);
    FUN_001365d0(object_handle, 0, 0);
    object_compute_node_matrices(object_handle);
    object_connect_to_map(object_handle, 0);
    FUN_0013e1a0(object_handle);
    FUN_0013c620(object_handle);
    object_compute_function_values(object_handle);
    object_compute_change_colors(object_handle);

    /* --- Widget and child attachment --- */
    {
      char *obj2 = (char *)object_get_and_verify_type(object_handle, -1);
      int obj_tag_idx = *(int *)obj2;
      tag_get(0x6f626a65, obj_tag_idx);
      FUN_00136150(object_handle);
    }
    attachments_new(object_handle);

    /* --- Restore the active flag --- */
    obj = (char *)object_get_and_verify_type(object_handle, -1);
    {
      uint32_t flags2 = *(uint32_t *)(obj + 0x4);
      if (saved_active_bit)
        flags2 |= 0x80000;
      else
        flags2 &= ~(uint32_t)0x80000;
      *(uint32_t *)(obj + 0x4) = flags2;
    }

    /* --- Conditionally wake the object --- */
    if ((*(uint8_t *)(header + 0x2) & 0x1) == 0 &&
        (*(uint32_t *)(obj + 0x4) & 0x80000) != 0 &&
        ((*(uint8_t *)(p + 0x4) & 0x2) == 0 ||
         *(int16_t *)(obj + 0x4c) != -1)) {
      object_delete(object_handle);
    }

    /* --- Creation effect --- */
    if (*(int *)(tag_data + 0xac) != -1) {
      FUN_0009ec30(*(int *)(tag_data + 0xac), object_handle, /* dup-args-ok */
                   object_handle, -1, 0, 0, 0, 0);
    }

    return object_handle;
  }

  /* --- Failure: free the allocated datum --- */
  FUN_0013c560(object_handle);
  object_postprocess_node_matrices(*(data_t **)0x5a8d50);
  object_handle = -1;

out_of_objects: {
  const char *name = tag_name_strip_path(tag_get_name(tag_index));
  crt_sprintf(local_buf, "OUT OF OBJECTS: cannot create %s", name);
  console_printf(0, "%s", local_buf);
  error(3, "%s", local_buf);
}
  return object_handle;
}

/*
 * object_attach_to_parent — attach a child object to a parent at a specific
 * node, establishing the parent-child relationship in the object hierarchy.
 *
 * First walks the parent's own parent chain to verify the child is not already
 * an ancestor of the parent (prevents circular attachment). Then computes the
 * inverse of the parent's node matrix and transforms the child's position,
 * up vector, and forward vector into the parent node's local coordinate space.
 * Stores the parent handle and node index in the child's object data
 * (offsets 0xCC and 0xD0). If the child was connected to the map (flag bit 11
 * of object_data_t.flags), it is disconnected before the transform and
 * reconnected afterward.
 *
 * Finally clears the "collideable" flag (bit 0) on the child's header if set,
 * sets the "updated this tick" flag (bit 4), and recomputes node matrices.
 *
 * Confirmed: 3 cdecl args (parent_handle, child_handle, parent_node_index).
 * Confirmed: CALL targets 0x13d680, 0x13fef0, 0x13fd00, 0x140eb0, 0x109150,
 *            0x109590, 0x109680, 0x140ce0, 0x119320, 0x141b70.
 * Confirmed: parent chain walk uses parent_object_index at offset 0xCC.
 * Confirmed: stores parent_handle at child+0xCC, node_index byte at child+0xD0.
 * Confirmed: flag test is (flags >> 0xB) & 1 — bit 11 of object_data_t.flags.
 * Inferred:  bit 11 means "connected to map" based on disconnect/reconnect
 * usage.
 */
void object_attach_to_parent(int parent_handle, int child_handle,
                             int parent_node_index)
{
  int iter;
  object_data_t *child_obj;
  uint8_t connected_to_map;
  float local_matrix[13]; /* 4x3 matrix = 52 bytes */
  float *node_mat;
  object_header_data_t *child_hdr;

  iter = parent_handle;

  /* Walk the parent chain to verify we are not creating a cycle. */
  while (iter != -1) {
    object_header_data_t *hdr =
      (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, iter);
    object_data_t *obj_iter = hdr->object;
    int16_t obj_type = obj_iter->type;

    if ((1 << ((uint8_t)obj_type & 0x1f)) == 0) {
      display_assert(csprintf((char *)0x5ab100,
                              "got an object type we didn't expect "
                              "(expected one of 0x%08x but got #%d).",
                              -1, (int)obj_type),
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }

    if (iter == child_handle)
      break;

    iter = obj_iter->parent_object_index.value;
  }

  if (iter != -1) {
    display_assert("cannot attach an object to one of its children",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x4c9, 1);
    system_exit(-1);
    return;
  }

  /* Get child and parent object pointers. */
  child_obj = (object_data_t *)object_get_and_verify_type(child_handle, -1);
  object_get_and_verify_type(parent_handle, -1);

  connected_to_map = (uint8_t)((child_obj->flags >> 0xB) & 1);

  if (!object_has_node(parent_handle, (int16_t)parent_node_index)) {
    display_assert("object_has_node(parent_object_index, parent_node_index)",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x4d3, 1);
    system_exit(-1);
  }

  /* Disconnect child from map if it was connected. */
  if (connected_to_map) {
    object_disconnect_from_map(child_handle);
  }

  /* Compute inverse of the parent node matrix, then transform the child's
     position, up, and forward vectors into the parent node's local space. */
  node_mat =
    (float *)object_get_node_matrix(parent_handle, (int16_t)parent_node_index);
  matrix_inverse(node_mat, local_matrix);
  matrix_transform_point(local_matrix,
                         (float *)&child_obj->unk_12, /* dup-args-ok */
                         (float *)&child_obj->unk_12);
  matrix_transform_vector(local_matrix,
                          (float *)&child_obj->unk_36, /* dup-args-ok */
                          (float *)&child_obj->unk_36);
  matrix_transform_vector(local_matrix,
                          (float *)&child_obj->unk_48, /* dup-args-ok */
                          (float *)&child_obj->unk_48);

  /* Store parent attachment info in the child object. */
  child_obj->parent_object_index.value = parent_handle;
  *(uint8_t *)((char *)child_obj + 0xD0) = (uint8_t)parent_node_index;

  /* Reconnect child to map if it was connected. */
  if (connected_to_map) {
    object_connect_to_map(child_handle, NULL);
  }

  /* Update child header flags. */
  child_hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, child_handle);
  object_get_and_verify_type(child_handle, -1);

  if (child_hdr->unk_2 & 0x01) {
    child_hdr->unk_2 &= 0xfe;
  }

  child_hdr =
    (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, child_handle);
  child_hdr->unk_2 |= 0x10;

  object_compute_node_matrices(child_handle);
}

/*
 * object_try_place — attempt to place an object at a new position by casting
 * a collision ray from the object's current position toward the target
 * position.
 *
 * Pushes a collision user stack entry (user=0x13), computes the delta vector
 * (current_pos - target_pos), then calls FUN_0014df70 to perform a collision
 * test along that ray. If the collision test succeeds or the object has no
 * current cluster placement (field 0x4c == -1), the function checks the
 * collision result for a valid surface. If a valid surface is found, it calls
 * object_translate to update the object's position and reconnect it to the map,
 * then recomputes node matrices. Returns true if the object was placed or
 * already had a valid cluster reference, false otherwise.
 *
 * Confirmed: cdecl, 2 stack args — PUSH position, PUSH handle before CALL.
 * Confirmed: returns bool in AL (callers TEST AL,AL after CALL).
 * Confirmed: collision_result buffer is 0x50 bytes (int16_t[40]).
 * Confirmed: collision user ID 0x13 pushed to stack at 0x5a8c80.
 * Confirmed: assert strings match "objects.c" at lines 0x93d and 0x953.
 */
bool object_try_place(int object_handle, float *position)
{
  char *obj;
  bool result;
  int16_t collision_result[40]; /* 0x50 bytes at EBP-0x5c */
  float delta[3]; /* 3 floats at EBP-0x0c */

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  result = false;

  /* Push collision user stack entry (user = 0x13). */
  if (*(volatile int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x93d, true);
    system_exit(-1);
  }
  {
    int depth = (int)*(volatile int16_t *)0x4761d8;
    *(volatile int16_t *)0x4761d8 += 1;
    *(int16_t *)(0x5a8c80 + depth * 2) = 0x13;
  }

  /* Compute delta vector: current_position - target_position. */
  delta[0] = *(float *)(obj + 0x0c) - position[0];
  delta[1] = *(float *)(obj + 0x10) - position[1];
  delta[2] = *(float *)(obj + 0x14) - position[2];

  /* Cast collision ray from target position along delta direction. */
  if (FUN_0014df70(0x1000e9, position, delta, -1, collision_result) ||
      *(int16_t *)(obj + 0x4c) == -1) {
    /* Collision found or object has no cluster placement. */
    if (*(int16_t *)((char *)collision_result + 0x10) == -1) {
      /* No valid surface in collision result — cannot place. */
      goto done;
    }
    /* Place object at collision surface position and reconnect to map. */
    object_translate(object_handle, (float *)((char *)collision_result + 0x18),
                     (void *)((char *)collision_result + 0x0c));
    object_compute_node_matrices(object_handle);
  }
  result = true;

done:
  /* Pop collision user stack entry. */
  if (*(volatile int16_t *)0x4761d8 <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x953, true);
    system_exit(-1);
  }
  *(volatile int16_t *)0x4761d8 -= 1;

  return result;
}

/*
 * object_update_children_recursive — recursively compute node matrices for an
 * object and all of its child objects.
 *
 * First computes the node matrices for the given object by calling
 * object_compute_node_matrices (0x141b70), then walks the child chain starting
 * at object_data+0xC8 (first child handle). For each child, verifies type via
 * datum_get + type check, recurses, then advances via next_object_index
 * (object_data+0xC4).
 *
 * Confirmed: CALL 0x13d680 with args (-1, handle) — object_get_and_verify_type.
 * Confirmed: CALL 0x141b70 with 1 arg (handle) — object_compute_node_matrices.
 * Confirmed: MOV ESI,[EDI+0xC8] — first child from object data.
 * Confirmed: datum_get(*(data_t**)0x5a8d50, child_handle) for child lookup.
 * Confirmed: MOVSX ECX,word ptr [EDI+0x64] — child object type (int16_t).
 * Confirmed: MOV ESI,[EDI+0xC4] — next sibling from child object data.
 * Confirmed: recursive self-call at 0x144719.
 */
void object_update_children_recursive(int object_handle)
{
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  int child_handle;

  /* compute node matrices for this object */
  object_compute_node_matrices(object_handle);

  /* walk the child object chain */
  child_handle = obj->unk_200.value;
  while (child_handle != -1) {
    object_header_data_t *child_header =
      (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, child_handle);
    object_data_t *child_obj = child_header->object;
    int16_t child_type = child_obj->type;

    if ((1 << ((uint8_t)child_type & 0x1f)) == 0) {
      char *msg =
        csprintf((char *)0x5ab100,
                 "got an object type we didn't expect (expected one of "
                 "0x%08x but got #%d).",
                 -1, (int)child_type);
      display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }

    object_update_children_recursive(child_handle);
    child_handle = child_obj->next_object_index.value;
  }
}

/*
 * object_attach_to_marker — attach a child object to a parent at a named
 * marker position.
 *
 * Resolves markers on both parent and child objects via
 * object_get_markers_by_string_id. Disconnects the child from the map, then:
 *
 * - If child_marker_name is NULL or empty: computes an inverse of the child
 *   marker matrix and transforms the parent marker's position/up/forward
 *   into the child's local frame, writing directly to the child object's
 *   position (offset 0x0C), up (0x24), and forward (0x30).
 *
 * - If child_marker_name is provided: delegates to
 *   object_compute_child_marker_position (0x141020) to compute the relative
 *   transform using both markers.
 *
 * Finally reconnects the child to the map and calls object_attach_to_parent
 * (0x144240) with the parent node index from the parent marker result.
 *
 * Confirmed: 4 cdecl args (PUSH count before CALL, ADD ESP,0x2c combined
 *            cleanup covers first 4 CALLs).
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with (-1,
 * child_handle). Confirmed: CALL 0x140f10 (object_get_markers_by_string_id)
 * twice, max_count=1. Confirmed: CALL 0x13fd00 (object_disconnect_from_map)
 * with child_handle. Confirmed: TEST EDI,EDI / CMP byte ptr [EDI],0 —
 * null-or-empty check on child_marker_name. Confirmed: CALL 0x109150
 * (matrix_inverse) with child_markers+4 as source. Confirmed: CALL 0x109590 /
 * 0x109680 transform into object+0xC, +0x24, +0x30. Confirmed: CALL 0x141020
 * (object_compute_child_marker_position) in else branch. Confirmed: CALL
 * 0x140ce0 (object_connect_to_map) with (child_handle, 0). Confirmed: CALL
 * 0x144240 (object_attach_to_parent) with (parent_handle, child_handle,
 * parent_markers[0]). Inferred:  marker result struct is 0x6C (108) bytes;
 * first dword is node index, matrix at offset +4, position/up/forward within
 * parent marker at offsets 0x60, 0x3C, 0x54 respectively.
 */
void object_attach_to_marker(int parent_handle, void *marker_name,
                             int child_handle, void *child_marker_name)
{
  char parent_markers[0x6C];
  char child_markers[0x6C];
  float inverse[13]; /* 4x3 matrix = 52 bytes */

  void *child_obj = object_get_and_verify_type(child_handle, -1);

  object_get_markers_by_string_id(parent_handle, marker_name, parent_markers,
                                  1);
  object_get_markers_by_string_id(child_handle, child_marker_name,
                                  child_markers, 1);
  object_disconnect_from_map(child_handle);

  if (child_marker_name == NULL || *(char *)child_marker_name == '\0') {
    /* No child marker name — invert the child marker's matrix and use it to
       transform the parent marker's position/up/forward into the child's
       local coordinate space. */
    matrix_inverse((float *)(child_markers + 4), inverse);
    matrix_transform_point(inverse, (float *)(parent_markers + 0x60),
                           (float *)((char *)child_obj + 0xC));
    matrix_transform_vector(inverse, (float *)(parent_markers + 0x3C),
                            (float *)((char *)child_obj + 0x24));
    matrix_transform_vector(inverse, (float *)(parent_markers + 0x54),
                            (float *)((char *)child_obj + 0x30));
  } else {
    /* Child marker name specified — delegate to
       object_compute_child_marker_position which handles the full
       relative-transform computation. The destination matrix aliases
       parent_markers+0x38 (the parent marker's embedded matrix). */
    object_compute_child_marker_position(child_obj, child_markers,
                                         parent_markers + 0x38);
  }

  object_connect_to_map(child_handle, NULL);
  object_attach_to_parent(parent_handle, child_handle, *(int *)parent_markers);
}

/*
 * object_delete_recursive — object deactivation and deallocation.
 *
 * Recursively tears down an object and its children/siblings, then deallocates
 * the object from the object pool. Called either from
 * objects_garbage_collection (immediate delete) or from the garbage collection
 * pass in objects_update.
 *
 * Steps:
 *   1. If object has flag 0x10000, clear garbage flag via
 * object_set_garbage_flag.
 *   2. Call deletion callbacks via FUN_00138eb0 (dispatch through function
 * table).
 *   3. Recursively deactivate child object (obj+0xC8).
 *   4. If delete_sibling is nonzero, recursively deactivate sibling (obj+0xC4).
 *   5. Clear collideable bit (datum header bit 0) if set.
 *   6. Call type table cleanup via FUN_0013c100.
 *   7. Call object cleanup via FUN_001362d0.
 *   8. Call widget detach via attachments_delete.
 *   9. If object has flag 0x800, disconnect from map via
 * object_disconnect_from_map.
 *  10. Call FUN_0013c560 (final cleanup).
 *  11. Free memory pool block if allocated (via memory_pool_block_free).
 *  12. Delete datum from object pool via datum_delete.
 *  13. Clear field_8 and unk_2 in header.
 *
 * Confirmed: cdecl, 2 stack args (object_handle, delete_sibling).
 * Confirmed: delete_sibling is read as byte (MOVZX AL) but compared as bool.
 * Confirmed: Recursive calls at 0x1449ff and 0x144a1c with (child/sibling, 1).
 * Confirmed: Multiple object_get_and_verify_type calls to re-fetch after
 * recursion. Confirmed: EDI preserved across recursive calls (initial object
 * ptr). Confirmed: obj+0xC8 is child handle, obj+0xC4 is sibling handle.
 * Confirmed: 0x10000 flag triggers garbage flag clear.
 * Confirmed: 0x800 flag triggers map disconnect.
 */
/* 0x1449b0 */
void object_delete_recursive(int object_handle, int delete_sibling)
{
  object_data_t *obj;
  object_header_data_t *hdr;
  int16_t obj_type;
  void *field_8_ptr;

  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  tag_get(0x6f626a65, (int)obj->tag_index);

  /* If object has flag 0x10000, clear the garbage flag. */
  if (obj->flags & 0x10000) {
    object_set_garbage_flag(object_handle, 0);
  }

  /* Dispatch deletion callbacks. */
  FUN_00138eb0(object_handle);

  /* Recursively deactivate child object. */
  if (obj->unk_200.value != -1) {
    object_delete_recursive(obj->unk_200.value, 1);
  }

  /* Optionally deactivate sibling object. */
  if ((char)delete_sibling != 0 && obj->next_object_index.value != -1) {
    object_delete_recursive(obj->next_object_index.value, 1);
  }

  /* Get datum header and clear collideable bit if set. */
  hdr = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  object_get_and_verify_type(object_handle, -1);
  if (hdr->unk_2 & 0x01) {
    hdr->unk_2 &= (uint8_t)~0x01;
  }

  /* Re-fetch object pointer after recursive calls. */
  obj = (object_data_t *)object_get_and_verify_type(object_handle, -1);
  tag_get(0x6f626a65, (int)obj->tag_index);

  /* Call type table cleanup. */
  obj_type = obj->type;
  FUN_0013c100(obj_type);

  /* Object cleanup and widget detach. */
  FUN_001362d0(object_handle);
  attachments_delete(object_handle);

  /* If flag 0x800 is set, disconnect from map. */
  if (obj->flags & 0x800) {
    object_disconnect_from_map(object_handle);
  }

  /* Final cleanup. */
  FUN_0013c560(object_handle);

  /* Free memory pool block if allocated. */
  hdr = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);
  field_8_ptr = (void *)&hdr->object;
  if (hdr->object != 0) {
    memory_pool_block_free(*(void **)0x46f080, (void **)field_8_ptr);
  }

  /* Delete datum from pool. */
  datum_delete(*(data_t **)0x5a8d50, object_handle);

  /* Clear remaining fields. */
  *(object_data_t **)field_8_ptr = 0;
  hdr->unk_2 = 0;
}

/*
 * objects_garbage_collection — delete and immediately deactivate an object.
 *
 * Marks the object (and its children) for deletion via object_delete_internal,
 * then immediately tears down / deallocates the object via
 * object_delete_recursive. Used by actor_erase_units as the "soft" deletion
 * path (flag!=0) as an alternative to object_delete, which only marks for
 * deletion and defers actual teardown to the objects_update garbage-collection
 * pass.
 *
 * Confirmed: cdecl, one stack arg (object_handle).
 * Confirmed: PUSH 0x0 / PUSH ESI / CALL 0x140bc0 (object_delete_internal).
 * Confirmed: PUSH 0x0 / PUSH ESI / CALL 0x1449b0 (object_delete_recursive).
 * Confirmed: ADD ESP,0x10 — combined cleanup for both 2-arg calls.
 * Confirmed: ESI saved/restored (callee-saved register for param_1).
 */
/* 0x144b30 */
void objects_garbage_collection(int object_handle)
{
  object_delete_internal(object_handle, 0);
  object_delete_recursive(object_handle, 0);
}

/*
 * objects_garbage_collect_tick — per-tick garbage collection pass.
 *
 * Runs each game tick from objects_update. Determines memory pressure level,
 * walks the garbage object list, deletes objects not visible to any player,
 * compacts the memory pool, and runs AI release callbacks when critical.
 *
 * Three GC levels: 0=forced (external flag), 1=mild (headroom low),
 * 2=critical (memory or slots exhausted). Callback table at 0x29b868 has
 * two AI release entries (swarms and encounters) plus a NULL terminator.
 *
 * Confirmed: void(void) cdecl, _chkstk for 0x2814 bytes of stack.
 * Confirmed: globals at 0x46f080 (pool), 0x46f084 (object_globals),
 *   0x5a8d50 (object_header_data), 0x5a8d4c (debug flag).
 * Confirmed: thresholds 0xcccc, 0x19999, 0x6666, 0x67, 0xCC, 0x32, 0x1E, 150.
 * Confirmed: three deletion calls in sequence: set_garbage_flag,
 * delete_internal, delete_recursive — all with (handle, 0). Confirmed: callback
 * table 2 entries: {NULL, 0x3fa40}, {0x3fb40, 0x3fc90}. Confirmed: FILD + FMUL
 * 100.0f + FMUL (1/1048576.0f) for percentage calc.
 */
/* 0x144b50 */
void objects_garbage_collect_tick(void)
{
  typedef struct {
    void (*init)(void *working_mem, uint16_t mem_size);
    int (*iterate)(char *result_desc, char *more_to_release, void *working_mem,
                   uint16_t mem_size);
  } gc_callback_entry_t;

  int garbage_handles[2048];
  char gc_working_mem[4096];
  char result_buf[512];
  char message_buf[512];
  char critical_buf[512];
  char status_buf[476];
  int gc_level_wide;
  int16_t garbage_object_count;
  int16_t gc_level;
  char did_callbacks;
  char timed_out;
  char init_called;
  char more_to_release;
  char should_delete;

  void *pool;
  object_globals_t *og;
  data_t *data;
  int contiguous_free;
  int free_size;
  int handle;
  int slots_free;
  object_header_data_t *hdr;
  object_data_t *obj;
  int16_t type;
  gc_callback_entry_t *callbacks;
  gc_callback_entry_t *entry;
  int previously_critical;

  pool = *(void **)0x46f080;
  og = object_globals;
  data = *(data_t **)0x5a8d50;
  callbacks = (gc_callback_entry_t *)0x29b868;

  /* Phase 1: determine GC level */
  if (og->garbage_collect_now) {
    gc_level = 0;
  } else {
    contiguous_free = memory_pool_get_contiguous_free_size(pool);
    if (contiguous_free <= (int)0xcccc) {
      memory_pool_compact(pool);
      contiguous_free = memory_pool_get_contiguous_free_size(pool);
      if (contiguous_free > (int)0x19999) {
        og->garbage_collect_now = 0;
        return;
      }
      gc_level = 2;
    } else if ((int16_t)(0x800 - *(int16_t *)((char *)data + 0x30)) < 0x67) {
      gc_level = 2;
    } else {
      if (og->unk_4 < 0x32)
        return;
      gc_level = 1;
    }
  }

  /* Phase 2: debug output */
  if (*(char *)0x5a8d4c) {
    contiguous_free = memory_pool_get_contiguous_free_size(pool);
    console_printf(0, "#%d objects using 0x%x bytes (0x%x contiguous free)",
                   (int)*(int16_t *)((char *)data + 0x2e),
                   (int)memory_pool_get_free_size(pool), contiguous_free);
  }

  /* Phase 3: build garbage object list */
  garbage_object_count = 0;
  handle = og->unk_8.value;
  while (handle != -1) {
    hdr = (object_header_data_t *)datum_get(data, handle);
    obj = hdr->object;
    type = obj->type;
    if ((1 << (type & 0x1f)) == 0) {
      display_assert(csprintf((char *)0x5ab100,
                              "got an object type we didn't expect (expected "
                              "one of 0x%08x but got #%d).",
                              -1, (int)type),
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
      system_exit(-1);
    }
    garbage_handles[garbage_object_count] = handle;
    garbage_object_count++;
    if (!(garbage_object_count < 2048)) {
      display_assert("garbage_object_count<MAXIMUM_OBJECTS_PER_MAP",
                     "c:\\halo\\SOURCE\\objects\\objects.c", 0x10c1, 1);
      system_exit(-1);
    }
    handle = (int)obj->unk_192;
  }

  /* Phase 4: decide whether to delete */
  gc_level_wide = (int)gc_level;
  should_delete = 0;
  switch (gc_level_wide) {
  case 0:
    should_delete = 0;
    break;
  case 1:
    should_delete = (og->unk_4 <= 30) ? 0 : 1;
    break;
  case 2:
    free_size = memory_pool_get_free_size(pool);
    if (free_size < (int)0x19999 ||
        (int16_t)(0x800 - *(int16_t *)((char *)data + 0x2e)) < (int16_t)0xcc) {
      should_delete = 0;
    } else {
      should_delete = 1;
      goto compact_and_callbacks;
    }
    break;
  default:
    display_assert("unreachable", "c:\\halo\\SOURCE\\objects\\objects.c",
                   0x10da, 1);
    system_exit(-1);
    break;
  }

  /* Phase 5: pop objects from list and attempt deletion */
  while (should_delete == 0 && garbage_object_count > 0) {
    garbage_object_count--;
    handle = garbage_handles[garbage_object_count];
    hdr = (object_header_data_t *)datum_get(data, handle);
    obj = hdr->object;

    if (gc_level == 1 && !(hdr->unk_2 & 1))
      continue;

    if (!object_visible_to_any_player(handle)) {
      type = obj->type;
      if ((1 << (type & 0x1f)) == 0) {
        display_assert(csprintf((char *)0x5ab100,
                                "got an object type we didn't expect (expected "
                                "one of 0x%08x but got #%d).",
                                -1, (int)type),
                       "c:\\halo\\SOURCE\\objects\\objects.c", 0x69a, 1);
        system_exit(-1);
      }
      if ((type & 3) <= 1) {
        if (obj->unk_182 & 4) {
          ai_debug_describe_actor(-1, handle, 0, (char *)0x5ab100, 256);
          error(2, "garbage collecting living unit: %s", (char *)0x5ab100);
        }
      }
      if (hdr->unk_2 & 1)
        og->unk_4--;
      object_set_garbage_flag(handle, 0);
      object_delete_internal(handle, 0);
      object_delete_recursive(handle, 0);
      should_delete = 1;
    }
  }

compact_and_callbacks:
  /* Phase 6: compact and run GC callbacks */
  memory_pool_compact(pool);

  if (*(char *)0x5a8d4c) {
    contiguous_free = memory_pool_get_contiguous_free_size(pool);
    console_printf(0, "compacted to #%d with 0x%x contiguous bytes free",
                   (int)*(int16_t *)((char *)data + 0x2e), contiguous_free);
  }

  if (should_delete) {
    og->garbage_collect_now = 0;
    return;
  }

  /* Determine timeout */
  timed_out = 0;
  if (og->last_garbage_collection_tick != (uint32_t)-1) {
    if (game_time_get() > (int)(og->last_garbage_collection_tick + 150))
      timed_out = 1;
  } else {
    timed_out = 1;
  }

  did_callbacks = 0;
  previously_critical = 0;
  entry = &callbacks[0];
  init_called = 0;

  /* Outer loop: check pressure and run callbacks */
  for (;;) {
    int is_critical;
    contiguous_free = memory_pool_get_contiguous_free_size(pool);
    slots_free = (int16_t)(0x800 - *(int16_t *)((char *)data + 0x2e));

    is_critical = 0;
    if (gc_level == 2) {
      if (contiguous_free < (int)0x6666 || slots_free < 0x33) {
        is_critical = 1;
        crt_sprintf(
          status_buf, "%.2f%% memory free, %d object slots free",
          (double)((float)contiguous_free * 100.0f * (1.0f / 1048576.0f)),
          slots_free);
      }
    }

    if (!is_critical && !previously_critical) {
      if (did_callbacks)
        goto finalize;
      if (timed_out) {
        if (contiguous_free < (int)0x6666 || slots_free < 0x33) {
          crt_sprintf(
            status_buf, "%.2f%% memory free, %d object slots free",
            (double)((float)contiguous_free * 100.0f * (1.0f / 1048576.0f)),
            slots_free);
          error(2, "garbage collection warning (%s)", status_buf);
        }
      }
      og->garbage_collect_now = 0;
      return;
    }

    /* Critical path */
    if (is_critical) {
      crt_sprintf(critical_buf, "garbage collection %scritical (%s)",
                  previously_critical ? "" : "now ", status_buf);
    } else {
      crt_sprintf(critical_buf, "garbage collection %scritical (%s)",
                  "no longer ", status_buf);
    }
    console_printf(0, "%s", critical_buf);
    error(3, "%s", critical_buf);
    did_callbacks = 1;

    if (is_critical && entry->iterate != NULL) {
      /* Inner loop: run callbacks */
      for (;;) {
        if (!init_called && entry->init != NULL) {
          entry->init(gc_working_mem, 0x1000);
          init_called = 1;
        }
        more_to_release = 0;
        if (entry->iterate(result_buf, &more_to_release, gc_working_mem,
                           0x1000)) {
          crt_sprintf(message_buf, "removing objects: %s", result_buf);
          console_printf(0, "%s", message_buf);
          error(3, "%s", message_buf);
          break;
        }
        if (!more_to_release) {
          entry++;
          init_called = 0;
          if (entry->iterate == NULL)
            goto finalize;
        }
      }
    } else {
      goto finalize;
    }

    previously_critical = 1;
    memory_pool_compact(pool);
  }

finalize:
  og->last_garbage_collection_tick = (uint32_t)game_time_get();
  og->garbage_collect_now = 0;
}

/*
 * objects_update — per-tick update for all active objects.
 *
 * Called once per game tick. Three passes over the object header array, plus
 * PVS comparison logic and a trailing garbage-collection call.
 *
 * Object header array base: *(void**)0x5a8d50 + 0x34.
 * Each element is 0xc bytes:
 *   +0x0 (int16_t): salt/generation (0 = slot empty)
 *   +0x2 (uint8_t): flags byte:
 *       bit 0 (0x01): collideable
 *       bit 2 (0x04): pending forced-update then deactivate
 *       bit 3 (0x08): pending deactivation
 *       bit 4 (0x10): "updated this tick" — cleared unconditionally each frame
 *       bit 5 (0x20): active (scheduled for update)
 *       bit 6 (0x40): PVS-relevant (cluster assigned)
 *       bit 7 (0x80): non-negative guard for non-collideable activation path
 *   +0x3 (uint8_t): object type index (used for double-speed skip mask)
 *   +0x4 (int16_t): cluster_index (-1 = NONE)
 *   +0x8 (uint32_t*): pointer to object_data_t
 *
 * PVS phase (only when PVS changes):
 *   og+0x4c (curr_pvs) receives this frame's combined player PVS; og+0xc
 *   (prev_pvs) receives the previous frame's curr_pvs snapshot.
 *   csmemcmp(prev_pvs, curr_pvs, pvs_size) detects a change.
 *   When they differ, walks all headers where bits 5 and 6 are both set
 *   (active + PVS-relevant):
 *     - Collideable (bit 0): if cluster NOT in curr_pvs:
 *         if [obj_data+4] & 0x80000 → FUN_140bc0(idx,0) (force-delete)
 *         else                      → FUN_13fb80(idx)   (deactivate)
 *     - Non-collideable (bit 0 clear, bit 7 clear), cluster != -1:
 *         if cluster IS in curr_pvs → FUN_13fb30(idx) (activate)
 *   Then calls FUN_1963c0(prev_pvs, curr_pvs, cluster_count) to update decals.
 *
 * Update phase:
 *   For each root object (flags & 1 set, flags & 4 clear):
 *     Asserts parent_object_index == -1 and next_object_index == -1.
 *     If game_players_are_double_speed() and object type is biped or vehicle
 *     (type bit 0 or 1 set) AND obj->field_1c8 != -1: skip FUN_1444f0.
 *     Otherwise: calls FUN_1444f0(handle) — object_update per-tick.
 *
 * Post-update phase:
 *   For each valid slot:
 *     Unconditionally clears bit 4 (0x10) from flags.
 *     If bit 2 (0x04) was set: clears bit 2, calls FUN_1444f0(handle) (flush).
 *     If bit 3 (0x08) set: calls FUN_1449b0(handle, 0) (deactivate/delete).
 *
 * Trailing call: FUN_144b50() — garbage-collect dead/stranded objects.
 *
 * Profiling markers: profile_enter_private / profile_exit_private around the
 * whole function, gated on two byte flags at 0x449ef1 and 0x324640.
 *
 * Confirmed: stride 0xc — ADD ESI,0xc at every loop-bottom.
 * Confirmed: element count = *(int16_t*)(obj_data_ptr+0x2e); compared with BX.
 * Confirmed: datum handle built as (int16_salt << 16) | int16_index via
 *            MOVSX + SHL 0x10 + OR.
 * Confirmed: EBX held as -1 sentinel throughout loop 2 (OR EBX,0xffffffff).
 * Confirmed: csmemcpy (0x8e0b0) copies old PVS to new PVS buffer and vice
 *            versa; players_get_combined_pvs (0xba6c0) provides current PVS.
 * Confirmed: ADD ESP,0xc (3 args) after first two csmemcpy calls; ADD ESP,0x18
 *            (6 args) after csmemcmp + csmemcpy combined cleanup.
 * Confirmed: MOVSX EAX,word ptr [EAX+0x134] — cluster count from scenario.
 * Confirmed: MOVSX EBX,BX / MOVSX ECX,DX used to zero-extend the 16-bit loop
 *            counter before PUSH as datum handle low word.
 * Confirmed: MOV AL,byte ptr [EBP-0x1] — double-speed bool held in stack slot.
 * Confirmed: display_assert + system_exit pattern identical to other functions.
 * Confirmed: FUN_13d680 called as object_get_and_verify_type(handle, -1) for
 *            the parent/next asserts, and (handle, 3) for the type mask check.
 * Confirmed: ADD ESP,0x8 after each 2-arg callee; ADD ESP,0x14 after each
 *            display_assert (4 args) + system_exit (1 arg) block.
 */
void objects_update(void)
{
  bool double_speed;
  object_globals_t *og;
  uint8_t *prev_pvs;
  uint8_t *curr_pvs;
  void *scen;
  int16_t cluster_count_raw;
  int pvs_size;
  void *combined_pvs;
  int pvs_changed;

  /* --- profiling entry (gated on two flags) --- */
  if ((*(volatile uint8_t *)0x449ef1 != 0) &&
      (*(volatile uint8_t *)0x324640 != 0)) {
    profile_enter_private(*(void *volatile *)0x324638);
  }

  /* --- double-speed player flag --- */
  /* game_time_get() returns the current tick; bit 0 set → odd tick. */
  double_speed = false;
  if ((game_time_get() & 1) != 0) {
    /* game_players_are_double_speed: returns bool via AL */
    if (game_players_are_double_speed()) {
      double_speed = true;
    }
  }

  /* --- PVS setup --- */
  /* object_globals->pending_update_count (int16 at +0x4) = 0 each frame */
  og = object_globals;
  *(int16_t *)((uint8_t *)og + 0x4) = 0;

  /* prev_pvs = og+0xc  (EBX in disasm; holds previous frame's PVS after copy)
   * curr_pvs = og+0x4c (EDI in disasm; receives fresh combined PVS each frame)
   * Confirmed: LEA EBX,[EAX+0xc]; MOV [EBP-0xc],EBX; LEA EDI,[EAX+0x4c]. */
  prev_pvs = (uint8_t *)og + 0xc;
  curr_pvs = (uint8_t *)og + 0x4c;

  /* cluster_count = scenario->bsp_cluster_count (int16 at scenario+0x134).
   * pvs_size = ((cluster_count + 0x1f) >> 5) << 2  (round up to dword).
   * Confirmed: MOVSX EAX,word [EAX+0x134]; MOVSX ESI,AX; ADD ESI,0x1f;
   *            SAR ESI,5; SHL ESI,2.
   * [EBP-8] holds the raw cluster_count_raw as int for later PUSH. */
  scen = scenario_get();
  cluster_count_raw = *(int16_t *)((uint8_t *)scen + 0x134);
  pvs_size = ((cluster_count_raw + 0x1f) >> 5) << 2;

  /* Step 1: save old curr_pvs into prev_pvs.
   * Confirmed: PUSH ESI(pvs_size);PUSH EDI(curr_pvs);PUSH EBX(prev_pvs);
   *            CALL csmemcpy; ADD ESP,0xc. */
  csmemcpy(prev_pvs, curr_pvs, pvs_size);

  /* Step 2: fetch this frame's combined player PVS; copy into curr_pvs.
   * players_get_combined_pvs() takes no arguments.
   * Confirmed: PUSH ESI (pre-push for next csmemcpy, not arg to pvs getter);
   *            CALL 0xba6c0; PUSH EAX(combined); PUSH EDI(curr_pvs);
   *            CALL csmemcpy. */
  combined_pvs = players_get_combined_pvs();
  csmemcpy(curr_pvs, combined_pvs, pvs_size);

  /* Step 3: compare prev vs curr — nonzero means PVS changed this tick.
   * Confirmed: PUSH ESI;PUSH EDI(curr_pvs);PUSH EBX(prev_pvs);
   *            CALL 0x8da40 (csmemcmp); ADD ESP,0x18 cleans steps 2+3. */
  pvs_changed =
    ((int (*)(void *, void *, int))0x8da40)(prev_pvs, curr_pvs, pvs_size);

  /* --- PVS-driven activation/deactivation pass --- */
  if (pvs_changed != 0) {
    data_t *obj_data = *(data_t **)0x5a8d50;
    /* Array base: *(void**)(obj_data+0x34); count: *(int16_t*)(obj_data+0x2e).
     * Confirmed: MOV ESI,[EAX+0x34]; XOR EBX,EBX; CMP word [EAX+0x2e],BX */
    uint8_t *hdr = *(uint8_t **)((uint8_t *)obj_data + 0x34);
    int16_t count = *(int16_t *)((uint8_t *)obj_data + 0x2e);
    int16_t i;
    for (i = 0; i < count; i++, hdr += 0xc) {
      uint8_t flags;
      /* Reload count from live pointer at loop bottom.
       * Confirmed: MOV ECX,[0x5a8d50]; CMP BX,word [ECX+0x2e] at 0x1452d8. */
      count = *(int16_t *)((uint8_t *)(*(data_t **)0x5a8d50) + 0x2e);

      /* Skip empty slots */
      if (*(int16_t *)hdr == 0)
        continue;

      flags = *(uint8_t *)(hdr + 0x2);

      /* Must have both PVS-relevant (0x40) and active (0x20) bits set */
      if ((flags & 0x40) == 0)
        continue;
      if ((flags & 0x20) == 0)
        continue;

      if ((flags & 0x1) != 0) {
        /* Collideable object: should always have a valid cluster_index.
         * Binary asserts cluster_index != -1 here. */
        int16_t cluster_idx = *(int16_t *)(hdr + 0x4);
        if (cluster_idx == -1) {
          display_assert("object_header->cluster_index!=NONE",
                         "c:\\halo\\SOURCE\\objects\\objects.c", 0x171, 1);
          system_exit(-1);
        }
        /* Check if cluster is in the current PVS bitmap (EDI = curr_pvs).
         * Confirmed: SAR EAX,5; TEST [EDI+EAX*4],EDX; JNZ skip. */
        if ((*(uint32_t *)(curr_pvs + ((cluster_idx >> 5) * 4)) &
             (1u << (cluster_idx & 0x1f))) == 0) {
          /* Cluster is NOT in current PVS — deactivate or force-delete.
           * [ESI+8] = pointer to object_data; check object_data[1] & 0x80000.
           * Confirmed: MOV EAX,[ESI+8]; TEST dword [EAX+4],0x80000. */
          uint32_t *obj_dat = *(uint32_t **)(hdr + 0x8);
          if ((obj_dat[1] & 0x80000) != 0) {
            /* Has "always update" flag: force-delete. */
            object_delete_internal((int)i, 0);
          } else {
            /* Normal deactivate via object_deactivate. */
            object_deactivate((int)i);
          }
        }
      } else {
        /* Non-collideable path: activate if cluster is now in PVS.
         * Bit 7 of flags guards this path (skip if negative).
         * Confirmed: TEST AL,AL; JS skip. */
        int16_t cluster_idx;
        if ((flags & 0x80) != 0)
          continue;

        cluster_idx = *(int16_t *)(hdr + 0x4);
        if (cluster_idx == (int16_t)-1)
          continue;

        /* Check if cluster IS in current PVS.
         * Confirmed: TEST [EDI+EAX*4],EDX; JZ skip. */
        if ((*(uint32_t *)(curr_pvs + ((cluster_idx >> 5) * 4)) &
             (1u << (cluster_idx & 0x1f))) != 0) {
          object_activate((int)i);
        }
      }
    }

    /* Update structure decals for changed PVS.
     * FUN_1963c0(prev_pvs, curr_pvs, cluster_count): 3 cdecl args.
     * Confirmed: PUSH EDX([EBP-8]=cluster_count_raw cast to int),
     *            PUSH EDI(curr_pvs), PUSH EAX([EBP-0xc]=prev_pvs);
     *            CALL 0x1963c0; ADD ESP,0xc. */
    ((void (*)(void *, void *, int))0x1963c0)(prev_pvs, curr_pvs,
                                              (int)cluster_count_raw);
  }

  /* --- per-object update pass (root objects only) --- */
  /* Reload array base — may have been invalidated by the PVS pass.
   * Confirmed: MOV EAX,[0x5a8d50]; MOV EDI,[EAX+0x34] at 0x1452fd. */
  {
    data_t *obj_data = *(data_t **)0x5a8d50;
    uint8_t *hdr = *(uint8_t **)((uint8_t *)obj_data + 0x34);
    int16_t count = *(int16_t *)((uint8_t *)obj_data + 0x2e);
    int16_t i;

    for (i = 0; i < count; i++, hdr += 0xc) {
      uint8_t flags;
      int16_t salt;
      int handle;
      bool do_update;
      /* Reload count each iteration (confirmed at 0x1453de-0x1453eb).
       * Confirmed: MOV EAX,[0x5a8d50]; ... CMP DX,word [EAX+0x2e] */
      count = *(int16_t *)((uint8_t *)(*(data_t **)0x5a8d50) + 0x2e);

      if (*(int16_t *)hdr == 0)
        continue;

      flags = *(uint8_t *)(hdr + 0x2);
      /* Must be active (bit 0) and not pending forced-update (bit 2) */
      if ((flags & 0x1) == 0)
        continue;
      if ((flags & 0x4) != 0)
        continue;

      /* Build datum handle: (salt << 16) | index.
       * Confirmed: MOVSX ESI,CX (salt); MOVSX ECX,DX (index); SHL ESI,0x10;
       *            OR EBX,0xffffffff; OR ESI,ECX. */
      salt = *(int16_t *)hdr;
      handle = (int)(((uint32_t)(uint16_t)salt << 16) | (uint16_t)i);

      /* Assert: object must be a root (parent == -1) */
      {
        object_data_t *obj =
          (object_data_t *)object_get_and_verify_type(handle, 0xffffffff);
        if (*(int *)((uint8_t *)obj + 0xcc) != -1) {
          display_assert(
            "object_get(object_index)->object.parent_object_index==NONE",
            "c:\\halo\\SOURCE\\objects\\objects.c", 0x1a0, 1);
          system_exit(-1);
        }
      }

      /* Assert: object must not have a next sibling (next == -1) */
      {
        object_data_t *obj =
          (object_data_t *)object_get_and_verify_type(handle, 0xffffffff);
        if (*(int *)((uint8_t *)obj + 0xc4) != -1) {
          display_assert(
            "object_get(object_index)->object.next_object_index==NONE",
            "c:\\halo\\SOURCE\\objects\\objects.c", 0x1a1, 1);
          system_exit(-1);
        }
      }

      /* Double-speed skip: if double_speed && this object's type is biped or
       * vehicle (type-bit 0 or 1) && obj->field_1c8 != -1 → skip update.
       * Confirmed: MOV CL,[EDI+3] (type byte); SHL EDX,CL; TEST DL,0x3. */
      do_update = true;
      if (double_speed) {
        uint8_t type_byte = *(uint8_t *)(hdr + 0x3);
        if (((1u << (type_byte & 0x1f)) & 0x3) != 0) {
          object_data_t *obj =
            (object_data_t *)object_get_and_verify_type(handle, 3);
          if (*(int *)((uint8_t *)obj + 0x1c8) != -1) {
            do_update = false;
          }
        }
      }

      if (do_update) {
        ((int (*)(int))0x1444f0)(handle);
      }
    }
  }

  /* --- post-update flag cleanup and deferred operations --- */
  /* Confirmed: XOR EDI,EDI (index counter); MOV BL,0xef (& mask for bit 4).
   * Reload base: MOV ESI,[EAX+0x34] at 0x1453f4 after loop 2. */
  {
    data_t *obj_data = *(data_t **)0x5a8d50;
    uint8_t *hdr = *(uint8_t **)((uint8_t *)obj_data + 0x34);
    int16_t count = *(int16_t *)((uint8_t *)obj_data + 0x2e);
    int16_t i;

    for (i = 0; i < count; i++, hdr += 0xc) {
      uint8_t flags;
      /* Reload count each iteration.
       * Confirmed: MOV ECX,[0x5a8d50]; CMP DI,word [ECX+0x2e] at 0x14544a. */
      count = *(int16_t *)((uint8_t *)(*(data_t **)0x5a8d50) + 0x2e);

      if (*(int16_t *)hdr == 0)
        continue;

      /* Read flags, clear bit 4 (0x10) unconditionally ("updated this tick").
       * Confirmed: MOV AL,[ESI+2]; AND AL,0xef; MOV [ESI+2],AL. */
      flags = *(uint8_t *)(hdr + 0x2);
      flags &= (uint8_t)0xef;
      *(uint8_t *)(hdr + 0x2) = flags;

      /* If bit 2 (0x04) was set before the AND (i.e. was set before clearing):
       * also clear bit 2, then call object_update (0x1444f0).
       * Confirmed: TEST AL,0x4; JZ ...; AND AL,0xfb; MOV [ESI+2],AL. */
      if ((flags & 0x4) != 0) {
        int16_t salt;
        int handle;
        flags &= (uint8_t)0xfb;
        *(uint8_t *)(hdr + 0x2) = flags;
        salt = *(int16_t *)hdr;
        handle = (int)(((uint32_t)(uint16_t)salt << 16) | (uint16_t)i);
        ((int (*)(int))0x1444f0)(handle);
      }

      /* If bit 3 (0x08) is set: deactivate/delete the object.
       * Confirmed: TEST byte [ESI+2],0x8; JZ ...; ... CALL 0x1449b0. */
      if ((*(uint8_t *)(hdr + 0x2) & 0x8) != 0) {
        int16_t salt = *(int16_t *)hdr;
        int handle = (int)(((uint32_t)(uint16_t)salt << 16) | (uint16_t)i);
        object_delete_recursive(handle, 0);
      }
    }
  }

  /* --- garbage collection --- */
  /* FUN_144b50: collects dead/stranded objects; no args; no return value.
   * Confirmed: bare CALL 0x144b50 at 0x14545a. */
  ((void (*)(void))0x144b50)();

  /* --- profiling exit --- */
  if ((*(volatile uint8_t *)0x449ef1 != 0) &&
      (*(volatile uint8_t *)0x324640 != 0)) {
    profile_exit_private(*(void *volatile *)0x324638);
  }
}

/* 0x1a9520 — get world-space position of the "body" marker on an object.
 * Thin wrapper: calls object_get_markers_by_string_id for marker "body",
 * then extracts XYZ from offset 0x60 in the marker output record. */
void FUN_001a9520(int object_handle, float *out_position)
{
  char marker_buf[0x6c];
  object_get_markers_by_string_id(object_handle, "body", marker_buf, 1);
  out_position[0] = *(float *)(marker_buf + 0x60);
  out_position[1] = *(float *)(marker_buf + 0x64);
  out_position[2] = *(float *)(marker_buf + 0x68);
}

/* FUN_00084fe0 (0x84fe0) — Set the bored-camera enable flag and mark the
 * camera state dirty so it will be re-evaluated this tick.
 * Object: objects.obj / source: bored_camera.c
 *
 * Confirmed: MOV AL,[param_1]; MOV [0x2ee5a0],AL; MOV byte ptr [0x2ee5a1],1.
 */
void FUN_00084fe0(unsigned char param_1)
{
  *(char *)0x2ee5a0 = param_1;
  *(char *)0x2ee5a1 = 1;
}

/* FUN_000850d0 (0x850d0) — Switch to first-person camera mode 2 for the
 * given unit handle, or report an error if the handle is -1.
 * Object: objects.obj / source: bored_camera.c
 *
 * Confirmed: CMP [param_1],-1; JE error_path; MOV [0x2ee5a2],2;
 * MOV [0x2ee5a1],1; MOV [0x2ee5d4],param_1; RET.
 */
void FUN_000850d0(int param_1)
{
  if (param_1 != -1) {
    *(char *)0x2ee5a2 = 2;
    *(char *)0x2ee5a1 = 1;
    *(int *)0x2ee5d4 = param_1;
    return;
  }
  error(2, "cannot set first person camera on a unit that doesn't exist.");
}

/* FUN_00085110 (0x85110) — Switch to first-person camera mode 3 for the
 * given unit handle, or report an error if the handle is -1.
 * Object: objects.obj / source: bored_camera.c
 *
 * Confirmed: identical to FUN_000850d0 but stores 3 in DAT_002ee5a2.
 */
void FUN_00085110(int param_1)
{
  if (param_1 != -1) {
    *(char *)0x2ee5a2 = 3;
    *(char *)0x2ee5a1 = 1;
    *(int *)0x2ee5d4 = param_1;
    return;
  }
  error(2, "cannot set first person camera on a unit that doesn't exist.");
}

/* FUN_00085180 (0x85180) — Configure camera globals from a cutscene-camera
 * entry (param_1) in the scenario, using param_2 as tick-based time and
 * param_3 as unit handle. Triggers director_update and observer_update.
 * Object: objects.obj / source: bored_camera.c
 *
 * Confirmed: CALL global_scenario_get; CALL tag_block_get_element(+0x4f0,param_1,0x68);
 * stores to 0x2ee5a1..0x2ee5d4; CALL vectors3d_from_euler_angles3d;
 * float compare for default speed (0x3f9c61aa); CALL director_update(0);
 * CALL observer_update(0x38d1b717).
 */
void FUN_00085180(short param_1, short param_2, int param_3)
{
  int iVar1;

  iVar1 = (int)tag_block_get_element((char *)global_scenario_get() + 0x4f0, (int)param_1, 0x68);
  *(short *)0x2ee5a2 = 0;
  *(char *)0x2ee5a1 = 1;
  *(short *)0x2ee5a4 = param_1;
  *(int *)0x2ee5ac = *(int *)(iVar1 + 0x28);
  *(int *)0x2ee5b0 = *(int *)(iVar1 + 0x2c);
  *(int *)0x2ee5b4 = *(int *)(iVar1 + 0x30);
  vectors3d_from_euler_angles3d((float *)0x2ee5b8, (float *)0x2ee5c4, (float *)(iVar1 + 0x34));
  if (*(float *)(iVar1 + 0x40) != *(float *)0x2533c0) {
    *(int *)0x2ee5d0 = *(int *)(iVar1 + 0x40);
  } else {
    *(int *)0x2ee5d0 = 0x3f9c61aa;
  }
  *(float *)0x2ee5a8 = (float)((int)param_2 / 0x1e);
  *(int *)0x2ee5d4 = param_3;
  director_update(0.0f);
  observer_update(9.9957275390625e-5f);
}
