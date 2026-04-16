/*
 * objects/objects.c — object system lifecycle and placement
 * XBE source: c:\halo\SOURCE\objects\objects.c
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x13d680  object_get_and_verify_type
 *   0x13f060  objects_place
 *   0x13f810  objects_initialize
 *   0x13f950  objects_initialize_for_new_map
 *   0x13f9f0  objects_dispose_from_old_map
 *   0x13fac0  objects_dispose
 *   0x145170  objects_update
 */

#include "common.h"

/* Forward declarations for unported callees in the same .obj cluster.
 * These are called via hardcoded addresses to avoid polluting kb.json
 * with stubs that would silently override thunks. */
typedef void (*pfn_void_t)(void);
typedef void (*pfn_int_t)(int);

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
            /* Has "always update" flag: force-delete via FUN_140bc0. */
            ((void (*)(int, int))0x140bc0)((int)i, 0);
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
