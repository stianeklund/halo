/*
 * objects/objects.c — object system lifecycle and placement
 * XBE source: c:\halo\SOURCE\objects\objects.c
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x13d640  object_try_and_get_and_verify_type
 *   0x13d680
 * object_get_and_verify_type
 *   0x13d920  object_set_garbage_flag
 * 0x13dfc0
 * object_header_block_reference_get
 *   0x13f060  objects_place
 *   0x13f810
 * objects_initialize
 *   0x13f950  objects_initialize_for_new_map
 * 0x13f9f0
 * objects_dispose_from_old_map
 *   0x13fac0  objects_dispose
 *   0x13fd00
 * object_disconnect_from_map
 *   0x13fef0  object_has_node
 *   0x13ffc0
 * object_set_garbage
 *   0x140160  object_set_region_count
 *   0x140230
 * object_adjust_interpolation_position
 *   0x140bc0  object_delete_internal
 *
 * 0x140cc0  object_delete 0x140ce0  object_connect_to_map 0x140eb0
 * object_get_node_matrix 0x140f10 object_get_markers_by_string_id 0x141020
 * object_compute_child_marker_position 0x1412f0  object_get_world_position
 *   0x141480  object_get_world_matrix
 *   0x141b70  object_compute_node_matrices
 *   0x1446a0  object_update_children_recursive
 *   0x144860  object_attach_to_marker
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
  int copy_size = (int)model_region_count << 5;
  void *src =
    object_header_block_reference_get(object_handle, (char *)obj + 0x19c);
  void *dst =
    object_header_block_reference_get(object_handle, (char *)obj + 0x198);
  csmemcpy(dst, src, copy_size);

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
    /* Propagate deletion to attached children. */
    object_propagate_flag_to_children(object_handle, 1, 0);
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
    if (location == NULL) {
      scenario_location_from_point(local_loc, (char *)obj + 0x50);
      location = local_loc;
      if ((int16_t)local_loc[1] == -1) {
        scenario_location_from_point(local_loc, (char *)obj + 0x0c);
      }
    }

    uint32_t *loc = (uint32_t *)location;
    if ((int16_t)loc[1] == -1) {
      obj->flags |= 0x200000;
    } else {
      obj->unk_72 = loc[0];
      obj->unk_76.value = loc[1];
      hdr->unk_4 = (uint16_t)loc[1];
      obj->flags &= ~0x200000u;
    }

    hdr->unk_2 &= 0x7f;

    object_data_t *self_obj =
      (object_data_t *)object_get_and_verify_type(object_handle, -1);
    void *obj_list =
      (self_obj->flags & 0x2000000) ? (void *)0x5a8d40 : (void *)0x5a8d30;
    cluster_partition_add_object(obj_list, object_handle, (char *)obj + 0xbc,
                                 (char *)obj + 0x50, *(uint32_t *)&obj->unk_92,
                                 (char *)obj + 0x48);

    if ((hdr->unk_2 & 0x40) != 0) {
      if (hdr->unk_4 != 0xffff) {
        int16_t cluster = (int16_t)hdr->unk_4;
        int *pvs = (int *)players_get_combined_pvs();
        if ((pvs[cluster >> 5] & (1 << (cluster & 0x1f))) != 0) {
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
  if (!object_has_node(object_handle, node_index)) {
    display_assert("object_has_node(object_index, node_index)",
                   "c:\\halo\\SOURCE\\objects\\objects.c", 0x424, 1);
    system_exit(-1);
  }
  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  char *nodes = (char *)object_header_block_reference_get(
    object_handle, (void *)&obj->unk_416);
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
  void *node_mat = object_get_node_matrix(object_handle, 0);
  qmemcpy((char *)out_markers + 0x38, node_mat, 0x34);

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

  assert_halt(object != NULL);
  assert_halt(child_marker != NULL);
  assert_halt(dest_matrix != NULL);
  assert_halt(((valid_real_matrix4x3_fn)0xf6d00)(dest_matrix));

  float *obj_position = (float *)((char *)object + 0xc);
  float *obj_forward = (float *)((char *)object + 0x24);
  float *obj_up = (float *)((char *)object + 0x30);

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
  float fwd_x = local_mat[1];
  float fwd_y = local_mat[2];
  float fwd_z = local_mat[3];
  float up_x = local_mat[7];
  float up_y = local_mat[8];
  float up_z = local_mat[9];

  /* left = cross(forward, up) */
  float left_x = fwd_y * up_z - fwd_z * up_y;
  float left_y = fwd_z * up_x - up_z * fwd_x;
  float left_z = up_y * fwd_x - fwd_y * up_x;

  /* up_new = cross(left, forward) */
  obj_up[0] = left_y * fwd_z - left_z * fwd_y;
  obj_up[1] = left_z * fwd_x - fwd_z * left_x;
  obj_up[2] = fwd_y * left_x - left_y * fwd_x;

  normalize3d(obj_forward);
  normalize3d(obj_up);
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

  if (obj->parent_object_index.value == NONE) {
    /* No parent — local position is the world position */
    *out_position = obj->unk_12;
    return out_position;
  }

  /* Parented — transform local position through parent's node matrix */
  void *node_mat = object_get_node_matrix(
    obj->parent_object_index.value, (int16_t) * (int8_t *)((char *)obj + 0xd0));
  matrix_transform_point((float *)node_mat, (float *)&obj->unk_12,
                         (float *)out_position);
  return out_position;
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
    matrix4x3_multiply(node_mat, out_matrix, out_matrix);
  }

  return out_matrix;
}

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
  /* Type-cast helpers for unported callees */
  typedef void (*object_type_validate_fn)(int16_t type);
  typedef void (*animation_set_default_fn)(void *model_tag, void *anim_data);
  typedef void (*animation_decode_fn)(void *model_tag, void *anim_entry,
                                      int frame_index, void *anim_data);
  typedef void (*animation_overlay_keyframe_fn)(
    void *anim_entry, float frame_value, void *anim_data);
  typedef void (*animation_overlay_interpolate_fn)(
    void *anim_entry, int frame_index, void *anim_data, void *node_data);
  typedef void (*overlay_adjust_fn)(int object_handle, void *anim_data);
  typedef void (*anim_interpolate_fn)(uint16_t node_count, void *interp_data,
                                      void *anim_data, int16_t frame_index,
                                      int16_t frame_count);
  typedef int (*valid_real_point3d_fn)(float *p);
  typedef int (*valid_real_vectors_fn)(float *fwd, float *left, float *up);
  typedef int (*valid_real_matrix4x3_fn)(float *m);
  typedef int (*valid_fwd_and_up_fn)(float *fwd, float *up);
  typedef void (*matrix_4x3_multiply_fn)(float *a, float *b, float *out);
  typedef void (*matrix_4x3_from_point_fn)(float *out, float *point);
  typedef void (*model_node_set_default_fn)(float *out, void *anim_data);

  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(object_handle, -1);
  void *object_tag = tag_get(0x6f626a65, *(int *)obj);
  float *node_matrices = (float *)object_header_block_reference_get(
    object_handle, (void *)((char *)obj + 0x1a0));

  /* Objects with type bits 5..11 set cannot interpolate and use a stack
   * buffer for animation data; others use the block reference at +0x19c. */
  uint8_t obj_type_byte = *(uint8_t *)((char *)obj + 0x64);
  int cannot_interpolate = ((1 << (obj_type_byte & 0x1f)) & 0xfe0u) != 0;
  void *anim_data;
  char anim_data_stack[2048];

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
    ((object_type_validate_fn)0x13c100)(*(int16_t *)((char *)obj + 0x64));
    void *model_tag = tag_get(0x6d6f6465, *(int *)((char *)object_tag + 0x34));

    /* Get parent node matrix if attached to a parent object */
    float *parent_node_mat = NULL;
    if (obj->parent_object_index.value != -1) {
      parent_node_mat = (float *)object_get_node_matrix(
        obj->parent_object_index.value,
        (int16_t) * (int8_t *)((char *)obj + 0xd0));
    }

    uint8_t override_decompressor = 0;

    /* Decode animation pose into anim_data */
    int anim_tag_index = *(int *)((char *)obj + 0x7c);
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
                if ((*(uint8_t *)region_block & 2) == 0) {
                  total_frames =
                    (float)(int)(*(int16_t *)((char *)overlay_anim_entry +
                                              0x22) -
                                 1);
                } else {
                  total_frames =
                    (float)(int)*(int16_t *)((char *)overlay_anim_entry + 0x22);
                }
                float frame_value = total_frames * func_value;
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
      if (cannot_interpolate) {
        display_assert("!TEST_FLAG(_object_mask_cannot_interpolate, "
                       "object->object.type)",
                       "c:\\halo\\SOURCE\\objects\\objects.c", 0xad9, 1);
        system_exit(-1);
      }
      void *interp_data = object_header_block_reference_get(
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
    uint16_t node_queue[64];
    int queue_read = 0;
    int queue_write = 1;
    node_queue[0] = 0;
    void *model_nodes_block = (char *)model_tag + 0xb8;

    float root_anim[13];
    float orientation_matrix[13];
    float translation_matrix[13];
    float phys_offset_matrix[13];
    float origin_matrix[13];
    float parent_copy[13];

    do {
      int16_t cur_read = (int16_t)queue_read;
      uint16_t node_idx_u16 = node_queue[cur_read];
      queue_read++;
      int node_idx = (int)(int16_t)node_idx_u16;
      void *node_data =
        tag_block_get_element(model_nodes_block, node_idx, 0x9c);

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
          ((matrix_4x3_multiply_fn)0x109850)(orientation_matrix, origin_matrix,
                                             orientation_matrix);

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
              orientation_matrix[10] *= pscale;
              orientation_matrix[11] *= pscale;
              orientation_matrix[12] *= pscale;
              /* Copy parent matrix to local buffer and set scale=1 */
              int k;
              float *src = parent_node_mat;
              float *dst = parent_copy;
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
              void *parent_obj =
                object_get_and_verify_type(obj->parent_object_index.value, -1);
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
                char *parent_name = (char *)tag_get_name(*(int *)parent_obj_2);
                char *context =
                  csprintf((char *)0x5ab100, "%s as parent node of %s",
                           parent_name, obj_name);

                /* assert_valid_real_matrix4x3 expanded inline */
                if ((*(uint32_t *)parent_node_mat & 0x7f800000) == 0x7f800000) {
                  char *msg =
                    csprintf((char *)0x5ab100, "%s had a bad scale %f", context,
                             (double)*parent_node_mat);
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
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
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
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
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
                    display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                   0xb37, 1);
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
              !((valid_real_vectors_fn)0xf6c40)(fwd, left, node_matrices + 7) ||
              (*(uint32_t *)&node_matrices[10] & 0x7f800000) == 0x7f800000 ||
              (*(uint32_t *)&node_matrices[11] & 0x7f800000) == 0x7f800000 ||
              (*(uint32_t *)&node_matrices[12] & 0x7f800000) == 0x7f800000) {
            /* Root node matrix invalid — dump diagnostic info */
            obj =
              (object_data_t *)object_get_and_verify_type(object_handle, -1);
            char *name = (char *)tag_get_name(*(int *)obj);
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
                    (double)parent_node_mat[10], (double)parent_node_mat[11],
                    (double)parent_node_mat[12]);
              error(2,
                    "                     scale (jason's ugly secret) "
                    "%f",
                    (double)*parent_node_mat);
            }

            error(2, "");

            error(2, "computed matrix fwd  %f %f %f", (double)node_matrices[1],
                  (double)node_matrices[2], (double)node_matrices[3]);
            error(2, "                left %f %f %f", (double)node_matrices[4],
                  (double)node_matrices[5], (double)node_matrices[6]);
            error(2, "                up   %f %f %f", (double)node_matrices[7],
                  (double)node_matrices[8], (double)node_matrices[9]);
            error(2, "                posn %f %f %f", (double)node_matrices[10],
                  (double)node_matrices[11], (double)node_matrices[12]);
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
                    (mag_fwd < 0 ? -mag_fwd : mag_fwd) >= *(float *)0x2549d8) {
                  char *msg = csprintf(
                    (char *)0x5ab100, "%s had a bad forward (%f,%f,%f)",
                    "object_compute_node_matrices root node matrix",
                    (double)fwd[0], (double)fwd[1], (double)fwd[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb69, 1);
                  system_exit(-1);
                }
              }
              {
                float mag_left = left[0] * left[0] + left[1] * left[1] +
                                 left[2] * left[2] - *(float *)0x2533c8;
                if ((*(uint32_t *)&mag_left & 0x7f800000) == 0x7f800000 ||
                    (mag_left < 0 ? -mag_left : mag_left) >=
                      *(float *)0x2549d8) {
                  char *msg =
                    csprintf((char *)0x5ab100, "%s had a bad left (%f,%f,%f)",
                             "object_compute_node_matrices root node matrix",
                             (double)left[0], (double)left[1], (double)left[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb69, 1);
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
                  char *msg =
                    csprintf((char *)0x5ab100, "%s had a bad up (%f,%f,%f)",
                             "object_compute_node_matrices root node matrix",
                             (double)node_matrices[7], (double)node_matrices[8],
                             (double)node_matrices[9]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb69, 1);
                  system_exit(-1);
                }
              }
              if ((*(uint32_t *)&node_matrices[10] & 0x7f800000) ==
                    0x7f800000 ||
                  (*(uint32_t *)&node_matrices[11] & 0x7f800000) ==
                    0x7f800000 ||
                  (*(uint32_t *)&node_matrices[12] & 0x7f800000) ==
                    0x7f800000) {
                char *msg =
                  csprintf((char *)0x5ab100, "%s had a bad position (%f,%f,%f)",
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
                  char *msg =
                    csprintf((char *)0x5ab100,
                             "%s had a forward (%f,%f,%f) not perpendicular "
                             "to left (%f,%f,%f)",
                             "object_compute_node_matrices root node matrix",
                             (double)fwd[0], (double)fwd[1], (double)fwd[2],
                             (double)left[0], (double)left[1], (double)left[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb69, 1);
                  system_exit(-1);
                }
              }
              {
                float dot_ul = left[0] * node_matrices[7] +
                               node_matrices[8] * left[1] +
                               node_matrices[9] * left[2];
                if ((*(uint32_t *)&dot_ul & 0x7f800000) == 0x7f800000 ||
                    (dot_ul < 0 ? -dot_ul : dot_ul) >= *(float *)0x2549d8) {
                  char *msg =
                    csprintf((char *)0x5ab100,
                             "%s had a up (%f,%f,%f) not perpendicular to "
                             "left (%f,%f,%f)",
                             "object_compute_node_matrices root node matrix",
                             (double)node_matrices[7], (double)node_matrices[8],
                             (double)node_matrices[9], (double)left[0],
                             (double)left[1], (double)left[2]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb69, 1);
                  system_exit(-1);
                }
              }
              {
                float dot_uf = node_matrices[7] * fwd[0] +
                               node_matrices[8] * fwd[1] +
                               node_matrices[9] * fwd[2];
                if ((*(uint32_t *)&dot_uf & 0x7f800000) == 0x7f800000 ||
                    (dot_uf < 0 ? -dot_uf : dot_uf) >= *(float *)0x2549d8) {
                  char *msg =
                    csprintf((char *)0x5ab100,
                             "%s had a forward (%f,%f,%f) not perpendicular "
                             "to up (%f,%f,%f)",
                             "object_compute_node_matrices root node matrix",
                             (double)fwd[0], (double)fwd[1], (double)fwd[2],
                             (double)node_matrices[7], (double)node_matrices[8],
                             (double)node_matrices[9]);
                  display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                                 0xb69, 1);
                  system_exit(-1);
                }
              }
              if ((*(uint32_t *)node_matrices & 0x7f800000) == 0x7f800000 ||
                  !((valid_real_vectors_fn)0xf6c40)(fwd, left,
                                                    node_matrices + 7) ||
                  !((valid_real_point3d_fn)0xa16b0)(node_matrices + 10)) {
                char *msg =
                  csprintf((char *)0x5ab100, "%s: assert_valid_real_matrix4x3",
                           "object_compute_node_matrices root node matrix");
                display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                               0xb69, 1);
                system_exit(-1);
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
              display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb77,
                             1);
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
                  (mag_left < 0 ? -mag_left : mag_left) >= *(float *)0x2549d8) {
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
                char *msg =
                  csprintf((char *)0x5ab100, "%s had a bad up (%f,%f,%f)",
                           name2, (double)node_matrices[7],
                           (double)node_matrices[8], (double)node_matrices[9]);
                display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c",
                               0xb77, 1);
                system_exit(-1);
              }
            }
            if ((*(uint32_t *)&node_matrices[10] & 0x7f800000) == 0x7f800000 ||
                (*(uint32_t *)&node_matrices[11] & 0x7f800000) == 0x7f800000 ||
                (*(uint32_t *)&node_matrices[12] & 0x7f800000) == 0x7f800000) {
              char *msg =
                csprintf((char *)0x5ab100, "%s had a bad position (%f,%f,%f)",
                         name2, (double)node_matrices[10],
                         (double)node_matrices[11], (double)node_matrices[12]);
              display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb77,
                             1);
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
                  (double)node_matrices[9], (double)left2[0], (double)left2[1],
                  (double)left2[2]);
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
                char *msg =
                  csprintf((char *)0x5ab100,
                           "%s had a forward (%f,%f,%f) not perpendicular "
                           "to up (%f,%f,%f)",
                           name2, (double)fwd2[0], (double)fwd2[1],
                           (double)fwd2[2], (double)node_matrices[7],
                           (double)node_matrices[8], (double)node_matrices[9]);
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
              display_assert(msg, "c:\\halo\\SOURCE\\objects\\objects.c", 0xb77,
                             1);
              system_exit(-1);
            }
          }
        }
      } else {
        /* Non-root node: set default pose then multiply by parent */
        float *node_mat = node_matrices + node_idx * 13;
        ((model_node_set_default_fn)0x109500)(node_mat, (char *)anim_data +
                                                          node_idx * 0x20);

        if (*(int16_t *)((char *)node_data + 0x24) == -1) {
          display_assert("node->parent_node_index!=NONE",
                         "c:\\halo\\SOURCE\\objects\\objects.c", 0xb71, 1);
          system_exit(-1);
        }
        int16_t parent_idx = *(int16_t *)((char *)node_data + 0x24);
        float *parent_mat = node_matrices + parent_idx * 13;
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

  /* Apply origin offset from model tag and set bounding sphere radius */
  matrix_transform_point(node_matrices, (float *)((char *)object_tag + 0x08),
                         (float *)((char *)obj + 0x50));
  float radius = *(float *)((char *)object_tag + 0x04);
  *(float *)((char *)obj + 0x5c) = radius;
  if (*(float *)((char *)obj + 0x60) > *(float *)0x2533c0) {
    *(float *)((char *)obj + 0x5c) = radius * *(float *)((char *)obj + 0x60);
  }
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
  int iter = parent_handle;

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
  object_data_t *child_obj =
    (object_data_t *)object_get_and_verify_type(child_handle, -1);
  object_get_and_verify_type(parent_handle, -1);

  uint8_t connected_to_map = (uint8_t)((child_obj->flags >> 0xB) & 1);

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
  float local_matrix[13]; /* 4x3 matrix = 52 bytes */
  float *node_mat =
    (float *)object_get_node_matrix(parent_handle, (int16_t)parent_node_index);
  matrix_inverse(node_mat, local_matrix);
  matrix_transform_point(local_matrix, (float *)&child_obj->unk_12,
                         (float *)&child_obj->unk_12);
  matrix_transform_vector(local_matrix, (float *)&child_obj->unk_36,
                          (float *)&child_obj->unk_36);
  matrix_transform_vector(local_matrix, (float *)&child_obj->unk_48,
                          (float *)&child_obj->unk_48);

  /* Store parent attachment info in the child object. */
  child_obj->parent_object_index.value = parent_handle;
  *(uint8_t *)((char *)child_obj + 0xD0) = (uint8_t)parent_node_index;

  /* Reconnect child to map if it was connected. */
  if (connected_to_map) {
    object_connect_to_map(child_handle, NULL);
  }

  /* Update child header flags. */
  object_header_data_t *child_hdr =
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

  /* compute node matrices for this object */
  object_compute_node_matrices(object_handle);

  /* walk the child object chain */
  int child_handle = obj->unk_200.value;
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
