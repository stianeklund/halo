/*
 * objects/objects.c — object system lifecycle and placement
 * XBE source: c:\halo\SOURCE\objects\objects.c
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x13d640  object_try_and_get_and_verify_type
 *   0x13d680  object_get_and_verify_type
 *   0x13d920  object_set_garbage_flag
 *   0x13f060  objects_place
 *   0x13f810  objects_initialize
 *   0x13f950  objects_initialize_for_new_map
 *   0x13f9f0  objects_dispose_from_old_map
 *   0x13fac0  objects_dispose
 *   0x13fd00  object_disconnect_from_map
 *   0x13ffc0  object_set_garbage
 *   0x140bc0  object_delete_internal
 *   0x140cc0  object_delete
 *   0x141480  object_get_world_matrix
 *   0x145170  objects_update
 */

#include "common.h"

/* Forward declarations for unported callees in the same .obj cluster.
 * These are called via hardcoded addresses to avoid polluting kb.json
 * with stubs that would silently override thunks. */
typedef void (*pfn_void_t)(void);
typedef void (*pfn_int_t)(int);

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
  object_header_data_t *header = datum_get(*(data_t **)0x5a8d50, datum_handle);
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
 */
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
    if ((obj->flags & 0x10000) == 0)
      goto done;

    /* Walk the list to find the previous pointer */
    uint32_t *prev_ptr = (uint32_t *)&og->unk_8.value;
    int cur = og->unk_8.value;

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

/*
 * objects_place — place all scenario objects for the current map.
 *
 * Sets the object_is_being_placed flag on object_globals, calls the scenario
 * object placer (FUN_0013cdd0, unported), then clears the flag.
 * The flag is at byte offset 0x00 of the object_globals struct.
 *
 * Confirmed: MOV byte ptr [EAX], 0x1 / MOV byte ptr [ECX], 0x0
 * Confirmed: global_scenario_get() result (EAX) pushed as sole arg to placer.
 * Confirmed: ADD ESP,0x4 after the placer call (1 cdecl arg).
 */
void objects_place(void)
{
  /* Set object_is_being_placed = true */
  object_globals->object_is_being_placed = 1;

  /* Get the scenario pointer and pass it to the object placer */
  scenario_t *scenario = global_scenario_get();
  ((pfn_int_t)0x13cdd0)((int)scenario);

  /* Clear object_is_being_placed */
  object_globals->object_is_being_placed = 0;
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
 *               main heap; FUN_0011e650("objects", &DAT_500000) — memory-
 *               pool_new from main heap using a size read from 0x500000.
 *
 * Sub-system init call order (confirmed from disasm):
 *   FUN_00136580 — unknown object sub-type A init
 *   FUN_00135f90 — unknown object sub-type B init
 *   FUN_0013c2e0 — object type definition list init
 *   FUN_001391e0 — object BSP cluster data init
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
 *   FUN_001392b0  — calls data_delete_all on a BSP cluster data table,
 *                   then object_list_initialize_for_new_map via FUN_1915d0
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

  object_globals_t *og = object_globals;

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
 *   FUN_001392e0  — per-map dispose for BSP cluster data
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
  ((pfn_void_t)0x1365b0)();
  ((pfn_void_t)0x1360a0)();
  ((pfn_void_t)0x13c400)();
  ((pfn_void_t)0x1392e0)();

  data_t *obj_data = *(data_t **)0x5a8d50;

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
 *   FUN_001392a0  — disposes the BSP cluster data, calls FUN_191630
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
    {
      int _eax = (int)((char *)parent_obj + 0xc8);
      int _ebx = object_handle;
      __asm__ __volatile__("call *%[fn]"
                           : "+a"(_eax), "+b"(_ebx)
                           : [fn] "r"((void *)0x13e510)
                           : "ecx", "edx", "esi", "edi", "memory", "cc");
    }
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
        {
          int args[2];
          args[0] = 0; /* param_1=0 */
          args[1] = 1; /* param_2=1 */
          __asm__ __volatile__("pushl 4(%[a])\n\t"
                               "pushl 0(%[a])\n\t"
                               "call *%[fn]\n\t"
                               "addl $8, %%esp"
                               :
                               : [a] "r"(args), [fn] "r"((void *)0x13ee60),
                                 "a"(object_handle)
                               : "ecx", "edx", "memory", "cc");
        }
        goto lab_0014000a;
      }
      /* bit0 set, flag==0: no child propagation needed */
      goto lab_0014000a;
    } else {
      /* bit0 is clear */
      if ((char)flag == 0) {
        /* Becoming inactive: propagate to children with (param_1=1,
         * param_2=0). */
        {
          int args[2];
          args[0] = 1; /* param_1=1 */
          args[1] = 0; /* param_2=0 */
          __asm__ __volatile__("pushl 4(%[a])\n\t"
                               "pushl 0(%[a])\n\t"
                               "call *%[fn]\n\t"
                               "addl $8, %%esp"
                               :
                               : [a] "r"(args), [fn] "r"((void *)0x13ee60),
                                 "a"(object_handle)
                               : "ecx", "edx", "memory", "cc");
        }
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
  void *tag_def = tag_get(0x6f626a65, (int)obj->tag_index);
  if (*(int *)((char *)tag_def + 0x34) != -1 && (obj->flags & 1) == 0) {
    /* Propagate deletion to attached children via FUN_0013ee60.
     * EAX = object_handle (register arg), stack args = (1, 0). */
    {
      int args[2];
      args[0] = 1;
      args[1] = 0;
      __asm__ __volatile__("pushl 4(%[a])\n\t"
                           "pushl 0(%[a])\n\t"
                           "call *%[fn]\n\t"
                           "addl $8, %%esp"
                           :
                           : [a] "r"(args), [fn] "r"((void *)0x13ee60),
                             "a"(object_handle)
                           : "ecx", "edx", "memory", "cc");
    }
  }

  /* Re-fetch datum header (recursive calls may have moved pool memory). */
  hdr = (object_header_data_t *)datum_get(*(data_t **)0x5a8d50, object_handle);

  /* Set obj->flags bit 0 (deleted/inactive). */
  obj->flags |= 1;

  /* Clear datum header bit 0x02 (active). */
  hdr->unk_2 &= (uint8_t)~0x02;

  /* Remove the object from the name list via FUN_0013eff0.
   * EDI = object_handle (register arg), no stack args. */
  __asm__ __volatile__("call *%[fn]"
                       :
                       : "D"(object_handle), [fn] "r"((void *)0x13eff0)
                       : "eax", "ecx", "edx", "memory", "cc");
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
    void *node_mat = ((void *(*)(int, int16_t))0x140eb0)(
      obj->parent_object_index.value,
      (int16_t) * (int8_t *)((char *)obj + 0xd0));
    ((void (*)(void *, void *, void *))0x109850)(node_mat, out_matrix,
                                                 out_matrix);
  }

  return out_matrix;
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
  /* --- profiling entry (gated on two flags) --- */
  if ((*(uint8_t *)0x449ef1 != 0) && (*(uint8_t *)0x324640 != 0)) {
    profile_enter_private(*(void **)0x324638);
  }

  /* --- double-speed player flag --- */
  /* game_time_get() returns the current tick; bit 0 set → odd tick. */
  bool double_speed = false;
  if ((game_time_get() & 1) != 0) {
    /* game_players_are_double_speed: returns bool via AL */
    if (game_players_are_double_speed()) {
      double_speed = true;
    }
  }

  /* --- PVS setup --- */
  /* object_globals->pending_update_count (int16 at +0x4) = 0 each frame */
  object_globals_t *og = object_globals;
  *(int16_t *)((uint8_t *)og + 0x4) = 0;

  /* prev_pvs = og+0xc  (EBX in disasm; holds previous frame's PVS after copy)
   * curr_pvs = og+0x4c (EDI in disasm; receives fresh combined PVS each frame)
   * Confirmed: LEA EBX,[EAX+0xc]; MOV [EBP-0xc],EBX; LEA EDI,[EAX+0x4c]. */
  uint8_t *prev_pvs = (uint8_t *)og + 0xc;
  uint8_t *curr_pvs = (uint8_t *)og + 0x4c;

  /* cluster_count = scenario->bsp_cluster_count (int16 at scenario+0x134).
   * pvs_size = ((cluster_count + 0x1f) >> 5) << 2  (round up to dword).
   * Confirmed: MOVSX EAX,word [EAX+0x134]; MOVSX ESI,AX; ADD ESI,0x1f;
   *            SAR ESI,5; SHL ESI,2.
   * [EBP-8] holds the raw cluster_count_raw as int for later PUSH. */
  void *scen = scenario_get();
  int16_t cluster_count_raw = *(int16_t *)((uint8_t *)scen + 0x134);
  int pvs_size = ((cluster_count_raw + 0x1f) >> 5) << 2;

  /* Step 1: save old curr_pvs into prev_pvs.
   * Confirmed: PUSH ESI(pvs_size);PUSH EDI(curr_pvs);PUSH EBX(prev_pvs);
   *            CALL csmemcpy; ADD ESP,0xc. */
  csmemcpy(prev_pvs, curr_pvs, pvs_size);

  /* Step 2: fetch this frame's combined player PVS; copy into curr_pvs.
   * players_get_combined_pvs() takes no arguments.
   * Confirmed: PUSH ESI (pre-push for next csmemcpy, not arg to pvs getter);
   *            CALL 0xba6c0; PUSH EAX(combined); PUSH EDI(curr_pvs);
   *            CALL csmemcpy. */
  void *combined_pvs = players_get_combined_pvs();
  csmemcpy(curr_pvs, combined_pvs, pvs_size);

  /* Step 3: compare prev vs curr — nonzero means PVS changed this tick.
   * Confirmed: PUSH ESI;PUSH EDI(curr_pvs);PUSH EBX(prev_pvs);
   *            CALL 0x8da40 (csmemcmp); ADD ESP,0x18 cleans steps 2+3. */
  int pvs_changed =
    ((int (*)(void *, void *, int))0x8da40)(prev_pvs, curr_pvs, pvs_size);

  /* --- PVS-driven activation/deactivation pass --- */
  if (pvs_changed != 0) {
    data_t *obj_data = *(data_t **)0x5a8d50;
    /* Array base: *(void**)(obj_data+0x34); count: *(int16_t*)(obj_data+0x2e).
     * Confirmed: MOV ESI,[EAX+0x34]; XOR EBX,EBX; CMP word [EAX+0x2e],BX */
    uint8_t *hdr = *(uint8_t **)((uint8_t *)obj_data + 0x34);
    int16_t count = *(int16_t *)((uint8_t *)obj_data + 0x2e);
    for (int16_t i = 0; i < count; i++, hdr += 0xc) {
      /* Reload count from live pointer at loop bottom.
       * Confirmed: MOV ECX,[0x5a8d50]; CMP BX,word [ECX+0x2e] at 0x1452d8. */
      count = *(int16_t *)((uint8_t *)(*(data_t **)0x5a8d50) + 0x2e);

      /* Skip empty slots */
      if (*(int16_t *)hdr == 0)
        continue;

      uint8_t flags = *(uint8_t *)(hdr + 0x2);

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
            /* Normal deactivate via FUN_13fb80. */
            ((void (*)(int))0x13fb80)((int)i);
          }
        }
      } else {
        /* Non-collideable path: activate if cluster is now in PVS.
         * Bit 7 of flags guards this path (skip if negative).
         * Confirmed: TEST AL,AL; JS skip. */
        if ((flags & 0x80) != 0)
          continue;

        int16_t cluster_idx = *(int16_t *)(hdr + 0x4);
        if (cluster_idx == (int16_t)-1)
          continue;

        /* Check if cluster IS in current PVS.
         * Confirmed: TEST [EDI+EAX*4],EDX; JZ skip. */
        if ((*(uint32_t *)(curr_pvs + ((cluster_idx >> 5) * 4)) &
             (1u << (cluster_idx & 0x1f))) != 0) {
          ((void (*)(int))0x13fb30)((int)i);
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

    for (int16_t i = 0; i < count; i++, hdr += 0xc) {
      /* Reload count each iteration (confirmed at 0x1453de-0x1453eb).
       * Confirmed: MOV EAX,[0x5a8d50]; ... CMP DX,word [EAX+0x2e] */
      count = *(int16_t *)((uint8_t *)(*(data_t **)0x5a8d50) + 0x2e);

      if (*(int16_t *)hdr == 0)
        continue;

      uint8_t flags = *(uint8_t *)(hdr + 0x2);
      /* Must be active (bit 0) and not pending forced-update (bit 2) */
      if ((flags & 0x1) == 0)
        continue;
      if ((flags & 0x4) != 0)
        continue;

      /* Build datum handle: (salt << 16) | index.
       * Confirmed: MOVSX ESI,CX (salt); MOVSX ECX,DX (index); SHL ESI,0x10;
       *            OR EBX,0xffffffff; OR ESI,ECX. */
      int16_t salt = *(int16_t *)hdr;
      int handle = ((int)(int16_t)salt << 16) | (int)(int16_t)i;

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
      bool do_update = true;
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

    for (int16_t i = 0; i < count; i++, hdr += 0xc) {
      /* Reload count each iteration.
       * Confirmed: MOV ECX,[0x5a8d50]; CMP DI,word [ECX+0x2e] at 0x14544a. */
      count = *(int16_t *)((uint8_t *)(*(data_t **)0x5a8d50) + 0x2e);

      if (*(int16_t *)hdr == 0)
        continue;

      /* Read flags, clear bit 4 (0x10) unconditionally ("updated this tick").
       * Confirmed: MOV AL,[ESI+2]; AND AL,0xef; MOV [ESI+2],AL. */
      uint8_t flags = *(uint8_t *)(hdr + 0x2);
      flags &= (uint8_t)0xef;
      *(uint8_t *)(hdr + 0x2) = flags;

      /* If bit 2 (0x04) was set before the AND (i.e. was set before clearing):
       * also clear bit 2, then call object_update (0x1444f0).
       * Confirmed: TEST AL,0x4; JZ ...; AND AL,0xfb; MOV [ESI+2],AL. */
      if ((flags & 0x4) != 0) {
        flags &= (uint8_t)0xfb;
        *(uint8_t *)(hdr + 0x2) = flags;
        int16_t salt = *(int16_t *)hdr;
        int handle = ((int)(int16_t)salt << 16) | (int)(int16_t)i;
        ((int (*)(int))0x1444f0)(handle);
      }

      /* If bit 3 (0x08) is set: deactivate/delete the object.
       * Confirmed: TEST byte [ESI+2],0x8; JZ ...; ... CALL 0x1449b0. */
      if ((*(uint8_t *)(hdr + 0x2) & 0x8) != 0) {
        int16_t salt = *(int16_t *)hdr;
        int handle = ((int)(int16_t)salt << 16) | (int)(int16_t)i;
        ((void (*)(int, int))0x1449b0)(handle, 0);
      }
    }
  }

  /* --- garbage collection --- */
  /* FUN_144b50: collects dead/stranded objects; no args; no return value.
   * Confirmed: bare CALL 0x144b50 at 0x14545a. */
  ((void (*)(void))0x144b50)();

  /* --- profiling exit --- */
  if ((*(uint8_t *)0x449ef1 != 0) && (*(uint8_t *)0x324640 != 0)) {
    profile_exit_private(*(void **)0x324638);
  }
}
