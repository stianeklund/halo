/* props.c — AI prop lifecycle and iteration.
 *
 * Corresponds to props.obj (XBE address range ~0x64100–0x64560).
 * Source path confirmed via __FILE__ string:
 *   c:\halo\SOURCE\ai\props.c
 *
 * Subsystem roles:
 *   props_initialize             (0x64100) — allocate "prop" data table via
 *                                            game_state_data_new (768 entries,
 *                                            0x138 bytes each)
 *   props_dispose                (0x64140) — empty stub, no teardown
 *   props_initialize_for_new_map (0x64150) — reset all prop entries
 *   props_dispose_from_old_map   (0x64160) — invalidate prop data table
 *   prop_iterator_new            (0x64540) — initialise a prop iterator
 *                                            for a given actor's prop chain
 *
 * Key globals:
 *   0x5ab23c  data_t *prop_data  — handle to the prop data table,
 *                                  written by game_state_data_new on init.
 *   0x6325a4  data_t *actor_data — shared actor pool (also used by actors.c).
 *
 * Data table parameters (confirmed from binary):
 *   name          = "prop"    (string at 0x25bb30)
 *   maximum_count = 0x300     (768 props)
 *   size          = 0x138     (prop_t struct size)
 *
 * Assert evidence:
 *   display_assert("prop_data", "c:\\halo\\SOURCE\\ai\\props.c", 36, 1)
 *   -> line 36 (0x24) confirms this is the first assert in the function.
 */

#include "../../common.h"

/* prop_data (0x5ab23c) and actor_data (0x6325a4) are declared in the
 * generated decl.h via the kb.json data entries and are visible here
 * through the common.h -> decl.h include chain. No re-declaration needed. */

/* 0x63e30 — thin wrapper over FUN_000639e0 (same scenario/bsp/origin/node/
 * target parameter family as sibling FUN_00063e90).
 *
 * Confirmed from disassembly (0x63e30-0x63e81):
 *   frame  = PUSH EBP / MOV EBP,ESP / SUB ESP,0x1c -> one 0x1c-byte local
 *            buffer at EBP-0x1c, passed to the callee as its result_buf.
 *   guard  = MOV EDI,[EBP+0x14] / CMP EDI,-1 / JE 0x63e7a; the taken branch
 *            does OR EAX,0xffffffff, i.e. `return -1`.
 *   call   = single CALL 0x639e0, cdecl, ADD ESP,0x1c (7 dwords / 7 args).
 *            Push order (last push = first C arg) is
 *            [EBP+8], [EBP+0xc], [EBP+0x10], EDI, ESI, -1, LEA EBP-0x1c,
 *            which matches FUN_000639e0's 7-arg kb prototype exactly.
 *            Note EAX is reused: LEA EAX,[EBP-0x1c] at 0x63e49 is pushed at
 *            0x63e4c BEFORE MOV EAX,[EBP+8] at 0x63e4d reloads it.
 *   out    = the three post-call reads are callee outputs inside result_buf,
 *            not independent locals (buffer-alias hazard):
 *              [EBP-0x18] = result_buf+0x04 -> target[0]
 *              [EBP-0x14] = result_buf+0x08 -> target[1]
 *              [EBP-0x0c] = result_buf+0x10 -> status
 *            Both target stores are plain 32-bit MOVs (MOV [ESI],ECX /
 *            MOV [ESI+4],EDX) — no x87 anywhere in this function.
 *   return = CMP EAX,-1 / JNE 0x63e7d returns EAX (status); the fall-through
 *            does MOV EAX,EDI, i.e. returns node_handle.
 *
 * Ghidra's decompile of this function is misleading in three ways: the stale
 * void(void) kb prototype turned all five parameters into in_stack_* pseudo-
 * args, it dropped every return value, and it sized the buffer as char[4]. */
int FUN_00063e30(int scenario, unsigned char bsp_idx, float *origin,
                 int node_handle, float *target)
{
  char result_buf[0x1c];
  int status;

  if (node_handle != -1) {
    FUN_000639e0(scenario, bsp_idx, origin, node_handle, target, -1,
                 result_buf);
    target[0] = *(float *)(result_buf + 0x04);
    target[1] = *(float *)(result_buf + 0x08);
    status = *(int *)(result_buf + 0x10);
    if (status == -1) {
      return node_handle;
    }
    return status;
  }
  return -1;
}

/* 0x64100 — props_initialize.
 * Allocates the prop data table. Called from ai_initialize.
 * Asserts (halt=true) if allocation fails, then calls system_exit(-1). */
void FUN_00064100(void)
{
  prop_data = game_state_data_new("prop", 0x300, 0x138);
  if (prop_data == 0) {
    display_assert("prop_data", "c:\\halo\\SOURCE\\ai\\props.c", 36, 1);
    system_exit(-1);
  }
}

/* 0x64140 — props_dispose.
 * Empty stub. Binary contains a single RET — no teardown needed. */
void FUN_00064140(void)
{
}

/* 0x64150 — props_initialize_for_new_map.
 * Deletes all prop entries (resets indices, clears active count).
 * Called before loading a new map so the pool is empty. */
void FUN_00064150(void)
{
  data_delete_all(prop_data);
}

/* 0x64160 — props_dispose_from_old_map.
 * Marks the prop data table as invalid (clears the valid signature).
 * Called when unloading a map. */
void FUN_00064160(void)
{
  data_make_invalid(prop_data);
}

/* 0x643d0 — allocate a prop and attach it to an actor.
 *
 * Allocates a slot in prop_data, initialises it via prop_add with no unit
 * handle (NONE), and returns the new prop handle.  Same head-of-function
 * idiom as prop_orphan_transition (0x648a0), which does the identical
 * data_new_at_index / prop_add(-1, actor_handle, new_handle) pair.
 *
 * No name is claimed: the binary carries no assert string or __FILE__ line
 * for this function, so the FUN_ symbol is retained.
 *
 * Frame (from disasm 0x643d0-0x643f6, 15 instructions):
 *   PUSH EBP / MOV EBP,ESP / PUSH ESI — no `sub esp`, so no locals; ESI is
 *   the only callee-saved register and holds the new prop handle across the
 *   second call.
 *
 * Ghidra's decompile is wrong in three ways and must not be transcribed:
 * the stale `void FUN_000643d0(void)` kb prototype hid the single [EBP+8]
 * parameter, the EAX return (via ESI) was dropped, and prop_add appeared
 * argument-less.  `OR EAX,0xffffffff` before the CALL is prop_add's @<eax>
 * register argument (unit_handle = NONE), not dead code.
 *
 * Call-site verification table:
 *   CALL 0x119610 data_new_at_index (cdecl, 1 stack arg):
 *     arg1 | MOV EAX,[0x5ab23c]; PUSH EAX | prop_data     | YES
 *     ret  | MOV ESI,EAX                  | prop_handle   | YES
 *   CALL 0x64170 prop_add (@<eax> + 2 stack args):
 *     arg1 | OR EAX,0xffffffff (@<eax>)   | -1            | YES
 *     arg2 | PUSH ECX (= MOV ECX,[EBP+8]) | actor_handle  | YES  (last push)
 *     arg3 | PUSH ESI                     | prop_handle   | YES
 *   The single `ADD ESP,0xc` after the second CALL is MSVC coalescing the
 *   cleanup for both calls (1 push + 2 pushes); it is not a 3-stack-arg
 *   prop_add, so the ARG_COUNT audit warning on 0x64170 is benign.
 *   return | MOV EAX,ESI | prop_handle | YES
 *
 * Store-offset table: none — this function writes no struct or stack buffer.
 */
int FUN_000643d0(int actor_handle)
{
  int prop_handle;

  prop_handle = data_new_at_index(prop_data);
  prop_add(-1, actor_handle, prop_handle);
  return prop_handle;
}

/* 0x64400 — prop_unlink_from_actor (@eax=actor_handle, @edi=prop_handle).
 *
 * Splices prop_handle out of the actor's singly-linked prop chain.  The chain
 * is rooted at actor+0x50 and linked through prop+0x8 (the next-handle field
 * confirmed by prop_iterator_next / FUN_00064570).
 *
 * Before unlinking, four NDEBUG assertions verify the prop is not still
 * referenced by any actor look-direction or idle-direction slot:
 *   - actor+0x270        : target prop index
 *   - actor+0x544/54c/550: secondary look direction (type==1 means prop)
 *   - actor+0x55c/56c/570: idle major direction
 *   - actor+0x55f/57c/580: idle minor direction
 *
 * Calling convention: register args loaded by the thunk —
 *   @<eax> = actor_handle, @<edi> = prop_handle.
 * In C this is a normal 2-argument function; the thunk handles register setup.
 *
 * Call-site verification table (from disassembly):
 *   Caller 0x64789 (prop_new_unacknowledged):
 *     MOV EAX,EBX   (EBX = actor_handle from [EBP+0x8]) -> @eax  YES
 *     CALL 0x64400  (EDI = prop_handle held in EDI)       -> @edi  YES
 *   Caller 0x64a80 (prop_detach):
 *     MOV EAX,[EBP+0x8]  actor_handle -> @eax             YES
 *     MOV EDI,[EBP+0xc]  prop_handle  -> @edi             YES
 *
 * Store-offset table (no struct is filled; fields are read for assertions
 * and a singly-linked pointer is updated):
 *   actor+0x50        : prop chain head handle (read & conditionally written)
 *   prop+0x8          : next-handle link (read and used as splice target) */
void FUN_00064400(int actor_handle, int prop_handle) /* @<eax>, @<edi> */
{
  char *actor;
  char *head_prop;
  char *cur_prop;
  int head_handle;
  int next_handle;
  int cur_handle;
  char *prev_next_field; /* pointer to the &prev->next field, for splice */

  actor = (char *)datum_get(actor_data, actor_handle);

  /* Assertion: prop must be the actor's current target prop. */
  if (((actor_t *)actor)->target_target_prop_index == prop_handle) {
    display_assert("actor->target.target_prop_index != prop_index",
                   "c:\\halo\\SOURCE\\ai\\props.c", 0x19b, 1);
    system_exit(-1);
  }

  /* Assertion: prop must not be the secondary look direction. */
  if ((((actor_t *)actor)->control_secondary_look_type != 0) &&
      (((actor_t *)actor)->control_secondary_look_direction_type == 1) &&
      (((actor_t *)actor)->control_secondary_look_direction_prop_index ==
       prop_handle)) {
    display_assert(
      "!((actor->control.secondary_look_type != _secondary_look_none) && "
      "(actor->control.secondary_look_direction.type == "
      "_direction_specification_prop) && "
      "(actor->control.secondary_look_direction.prop_index == prop_index))",
      "c:\\halo\\SOURCE\\ai\\props.c", 0x19e, 1);
    system_exit(-1);
  }

  /* Assertion: prop must not be the idle major direction. */
  if ((((actor_t *)actor)->control_idle_major_active != 0) &&
      (((actor_t *)actor)->control_idle_major_direction_type == 1) &&
      (((actor_t *)actor)->control_idle_major_direction_prop_index ==
       prop_handle)) {
    display_assert(
      "!((actor->control.idle_major_active) && "
      "(actor->control.idle_major_direction.type == "
      "_direction_specification_prop) && "
      "(actor->control.idle_major_direction.prop_index == prop_index))",
      "c:\\halo\\SOURCE\\ai\\props.c", 0x1a1, 1);
    system_exit(-1);
  }

  /* Assertion: prop must not be the idle minor direction. */
  if ((((actor_t *)actor)->control_idle_minor_active != 0) &&
      (((actor_t *)actor)->control_idle_minor_direction_type == 1) &&
      (((actor_t *)actor)->control_idle_minor_direction_prop_index ==
       prop_handle)) {
    display_assert(
      "!((actor->control.idle_minor_active) && "
      "(actor->control.idle_minor_direction.type == "
      "_direction_specification_prop) && "
      "(actor->control.idle_minor_direction.prop_index == prop_index))",
      "c:\\halo\\SOURCE\\ai\\props.c", 0x1a4, 1);
    system_exit(-1);
  }

  /* Splice prop_handle out of the singly-linked chain rooted at actor+0x50.
   * Chain links through prop+0x8 (confirmed from prop_iterator_next). */
  head_handle = ((actor_t *)actor)->field_050;
  head_prop = (char *)datum_get(prop_data, head_handle);

  if (((actor_t *)actor)->field_050 == prop_handle) {
    /* Removing the head: advance head to head->next. */
    ((actor_t *)actor)->field_050 = *(int *)(head_prop + 8);
    return;
  }

  /* Walk the chain until we find the node whose next == prop_handle. */
  cur_prop = head_prop;
  do {
    prev_next_field = cur_prop + 8; /* &cur_prop->next_handle */
    next_handle = *(int *)(cur_prop + 8);
    cur_prop = (char *)datum_get(prop_data, next_handle);
    cur_handle = *(int *)prev_next_field; /* re-read from the pointer */
  } while (cur_handle != prop_handle);

  /* prev->next = removed->next */
  *(int *)prev_next_field = *(int *)(cur_prop + 8);
}

/* 0x64540 — prop_iterator_new.
 * Initialises a prop iterator for the props associated with a given actor.
 *
 * Reads actor->field_0x50 (the actor's prop chain head handle) and stores it
 * into out[1] (out+4).  The caller (e.g. 0x12350) then passes *out to
 * FUN_00064570 to step through props one at a time.
 *
 * out[0] (out+0) is NOT written here — FUN_00064570 likely owns that slot.
 *
 * Store-offset table (derived from disasm, not decompiler):
 *   out+0: not written by this function
 *   out+4: actor->field_0x50 (prop chain head handle)
 *
 * Call-site verification:
 *   PUSH [EBP+0xc] (actor_handle) -> datum_get arg2
 *   PUSH [0x6325a4] (actor_data)  -> datum_get arg1
 *   MOV EDX,[EAX+0x50]            -> actor->field_0x50
 *   MOV [param_1+4],EDX           -> out[1]
 */
void FUN_00064540(int *out, int actor_handle)
{
  void *actor = datum_get(actor_data, actor_handle);
  out[1] = ((actor_t *)actor)->field_050;
}

/* 0x64570 — prop_iterator_next.
 * Advances a prop iterator and returns a pointer to the next prop record,
 * or NULL when the chain is exhausted.
 *
 * The iterator is a 2-slot int array (matches the layout used by
 * FUN_00064540 / FUN_00064540):
 *   iter[0] — current prop handle (written here before each datum_get)
 *   iter[1] — next prop handle    (updated to prop->field_0x8)
 *
 * Prop chain link field: prop+0x8 (next handle in singly-linked list).
 *
 * Call-site verification (disasm 0x64570):
 *   MOV ECX,[ESI+0x4]  → handle = iter[1]            YES
 *   MOV [ESI],ECX      → iter[0] = handle (current)  YES
 *   PUSH ECX           → datum_get arg2 (handle)      YES
 *   PUSH EAX ([0x5ab23c] = prop_data) → datum_get arg1  YES
 *   MOV ECX,[EAX+0x8]  → prop->next_handle            YES
 *   MOV [ESI+0x4],ECX  → iter[1] = next               YES
 *
 * Store-offset table (from disasm MOV [ESI+N]):
 *   ESI+0x0 : handle (iter[1] before call — becomes current)
 *   ESI+0x4 : prop->field_0x8 (next handle) */
int FUN_00064570(int *iter)
{
  int handle;
  char *prop;

  handle = iter[1];
  iter[0] = handle;
  if (handle == -1) {
    return 0;
  }
  prop = (char *)datum_get(prop_data, handle);
  iter[1] = *(int *)(prop + 8);
  return (int)prop;
}

/* 0x648a0 — prop_orphan_transition.
 *
 * Allocates a new prop with no unit handle, initializes it from prop_handle,
 * and links the parent/orphan records through prop+0xc. The source prop must
 * belong to actor_handle and must not already have an orphan.
 *
 * Call-site verification (disasm 0x648a0):
 *   data_new_at_index: PUSH [0x5ab23c] -> prop_data
 *   prop_add:          EAX=-1, PUSH ESI -> new handle,
 *                      PUSH [EBP+8] -> actor_handle
 *   FUN_000647c0:      EAX=EBX -> prop_handle,
 *                      PUSH ESI -> new handle,
 *                      PUSH [EBP+8] -> actor_handle
 *
 * Store offsets (from disasm):
 *   parent_prop+0x0c: new orphan handle
 *   orphan_prop+0x0c: parent prop handle
 */
int prop_orphan_transition(int actor_handle, int prop_handle)
{
  int orphan_handle;
  char *parent_prop;
  char *orphan_prop;

  orphan_handle = data_new_at_index(prop_data);
  prop_add(-1, actor_handle, orphan_handle);
  if (orphan_handle != -1) {
    parent_prop = (char *)datum_get(prop_data, prop_handle);
    orphan_prop = (char *)datum_get(prop_data, orphan_handle);
    if (*(int *)(parent_prop + 4) != actor_handle) {
      display_assert("parent_prop->owner_actor_index == actor_index",
                     "c:\\halo\\SOURCE\\ai\\props.c", 0x155, 1);
      system_exit(-1);
    }
    if (*(int *)(parent_prop + 0xc) != -1) {
      display_assert("parent_prop->orphan_prop_index == NONE",
                     "c:\\halo\\SOURCE\\ai\\props.c", 0x156, 1);
      system_exit(-1);
    }
    FUN_000647c0(prop_handle, actor_handle, orphan_handle);
    *(int *)(parent_prop + 0xc) = orphan_handle;
    *(int *)(orphan_prop + 0xc) = prop_handle;
  }
  return orphan_handle;
}

/* 0x64970 — prop_orphan_from_friend.
 *
 * Same allocate-and-link idiom as prop_orphan_transition (0x648a0), but the
 * new orphan is initialized from a *friend* prop rather than from the parent:
 * FUN_000647c0 receives friend_prop_handle in EAX, and the friend's 16-bit
 * state field is copied into the new orphan when it falls in [4,5].
 *
 * Call-site verification (disasm 0x64970):
 *   0x6497e data_new_at_index: PUSH [0x5ab23c] -> prop_data; result -> ESI
 *   0x6498d prop_add:          OR EAX,-1 (@eax unit_handle = -1),
 *                              PUSH EBX = [EBP+8]  -> actor_handle,
 *                              PUSH ESI            -> orphan_handle
 *   0x649aa datum_get:         PUSH EDX = prop_data, PUSH ECX = [EBP+0xc]
 *                              -> EDI = parent_prop
 *   0x649b8 datum_get:         PUSH EAX = prop_data, PUSH ESI
 *                              -> [EBP-4] = orphan_prop
 *   0x649cb datum_get:         PUSH EDX = prop_data, PUSH ECX = [EBP+0x10]
 *                              -> [EBP-8] = friend_prop
 *                              (one coalesced ADD ESP,0x18 covers all three)
 *   0x64a28 FUN_000647c0:      MOV EAX,[EBP+0x10] (@eax friend_prop_handle),
 *                              PUSH EBX -> actor_handle, PUSH ESI -> orphan
 *
 * Store offsets (from disasm, not the decompiler):
 *   [EDI+0x0c]      = ESI          parent_prop+0x0c = orphan_handle
 *   [[EBP-4]+0x0c]  = [EBP+0xc]    orphan_prop+0x0c = prop_handle
 *   MOV AX,word [[EBP-8]+0x24]; CMP AX,4/JL; CMP AX,5/JG;
 *   MOV word [[EBP-4]+0x24],AX     orphan_prop+0x24 = friend state (int16)
 *
 * Return: MOV EAX,ESI at 0x64a54 — the new orphan handle, shared by both the
 * taken and the untaken (orphan_handle == -1) paths.
 */
int prop_orphan_from_friend(int actor_handle, int prop_handle,
                            int friend_prop_handle)
{
  int orphan_handle;
  char *parent_prop;
  char *orphan_prop;
  char *friend_prop;
  short state;

  orphan_handle = data_new_at_index(prop_data);
  prop_add(-1, actor_handle, orphan_handle);
  if (orphan_handle != -1) {
    parent_prop = (char *)datum_get(prop_data, prop_handle);
    orphan_prop = (char *)datum_get(prop_data, orphan_handle);
    friend_prop = (char *)datum_get(prop_data, friend_prop_handle);
    if (*(int *)(parent_prop + 4) != actor_handle) {
      display_assert("parent_prop->owner_actor_index == actor_index",
                     "c:\\halo\\SOURCE\\ai\\props.c", 0x16d, 1);
      system_exit(-1);
    }
    if (*(int *)(parent_prop + 0xc) != -1) {
      display_assert("parent_prop->orphan_prop_index == NONE",
                     "c:\\halo\\SOURCE\\ai\\props.c", 0x16e, 1);
      system_exit(-1);
    }
    FUN_000647c0(friend_prop_handle, actor_handle, orphan_handle);
    *(int *)(parent_prop + 0xc) = orphan_handle;
    *(int *)(orphan_prop + 0xc) = prop_handle;
    state = *(short *)(friend_prop + 0x24);
    if (state >= 4 && state <= 5) {
      *(short *)(orphan_prop + 0x24) = state;
    }
  }
  return orphan_handle;
}

/* 0x64a80 — prop_detach.
 * Removes the prop record identified by prop_handle from the actor's prop
 * chain and then frees it from prop_data.
 *
 * Calls FUN_00064400 (@eax=actor_handle, @edi=prop_handle) to splice the
 * prop out of the actor's singly-linked chain, then datum_delete to free
 * the slot.
 *
 * Call-site verification (disasm 0x64a80):
 *   MOV EAX,[EBP+0x8]  → actor_handle → @eax for FUN_00064400  YES
 *   MOV EDI,[EBP+0xc]  → prop_handle  → @edi for FUN_00064400  YES
 *   CALL 0x64400                                                 YES
 *   MOV EAX,[0x5ab23c] → prop_data    → datum_delete arg1       YES
 *   PUSH EDI            → prop_handle  → datum_delete arg2       YES
 *   PUSH EAX            → datum_delete arg1                      YES
 *   CALL 0x1196d0                                                YES
 *   ADD ESP,0x8         → 2-arg cdecl cleanup                   YES */
void prop_iterator_next(int actor_handle, int prop_handle)
{
  FUN_00064400(actor_handle, prop_handle);
  datum_delete(prop_data, prop_handle);
}

/* 0x64ab0 — prop_find_by_object.
 *
 * Searches actor actor_handle's prop chain for a prop that references
 * object_handle (directly via prop+0x18) or references it indirectly via a
 * loaded weapon/parent object (prop+0x1c against the object's model target at
 * object+0x1a8 / object+0x1a4).
 *
 * Skips props whose state field (prop+0x24 as int16) is in the range [0, 1].
 * Only props with prop+0x24 < 0 or prop+0x24 > 1 are eligible.
 *
 * Returns the prop handle (int) of the first matching prop, or -1 if not found.
 *
 * param_1 = actor_handle, param_2 = object_handle (cdecl, no register args).
 *
 * Call-site verification table (disasm 0x64ab0):
 *   object_get_and_verify_type call:
 *     arg1 | PUSH EBX ([EBP+0xc]=object_handle) | object_handle | YES
 *     arg2 | PUSH 0x3                            | 3             | YES
 *   datum_get call:
 *     arg1 | PUSH ECX ([0x6325a4]=actor_data)    | actor_data    | YES
 *     arg2 | PUSH EAX ([EBP+0x8]=actor_handle)   | actor_handle  | YES
 *
 * Store-offset table: no struct filled; read-only traversal. */
int prop_get_active_by_unit_index(int actor_handle, int object_handle)
{
  char *obj;
  int target;
  char *actor;
  int cur_handle;
  int next_handle;
  char *cur_prop;
  short state;

  obj = (char *)object_get_and_verify_type(object_handle, 3);
  target = *(int *)(obj + 0x1a8);
  if (target == -1) {
    target = *(int *)(obj + 0x1a4);
  }

  actor = (char *)datum_get(actor_data, actor_handle);
  cur_handle = ((actor_t *)actor)->field_050; /* prop chain head */

  for (;;) {
    if (cur_handle == -1) {
      return -1;
    }

    cur_prop = (char *)datum_get(prop_data, cur_handle);
    next_handle = *(int *)(cur_prop + 8);
    state = *(short *)(cur_prop + 0x24);

    /* Skip props in state [0, 1] — only check states < 0 or > 1. */
    if (state >= 0 && state <= 1) {
      cur_handle = next_handle;
      continue;
    }

    /* Direct object reference match. */
    if (*(int *)(cur_prop + 0x18) == object_handle) {
      return cur_handle;
    }

    /* Indirect match via loaded weapon/parent on prop+0x1c. */
    if ((*(char *)(cur_prop + 0x14) != 0) &&
        (*(int *)(cur_prop + 0x1c) != -1) &&
        (*(int *)(cur_prop + 0x1c) == target)) {
      return cur_handle;
    }

    cur_handle = next_handle;
  }
}

/* 0x64ee0 — TIFFClose (libtiff 3.x).
 *
 * NOTE: This function is from c:\halo\SOURCE\bitmaps\libtiff\tif_close.c, NOT
 * from ai\props.c.  It was incorrectly grouped under props.obj in kb.json.
 * It is placed here because it falls between props.obj and tif_dir.obj in the
 * binary address space and has no separate tif_close.c TU in this project.
 * Callers are in tiff_file.c (bitmaps subsystem) — tif_close.c was folded
 * into the surrounding TU at link time.
 *
 * Closes a TIFF file and releases all resources:
 *   1. If tif->tif_flags (tif+0x6, word) != 0, call TIFFFlush / write-back.
 *   2. If tif->tif_cleanup (tif+0x11c, fn ptr) != NULL, call it.
 *   3. Call TIFFFreeDirectory (TIFFFreeDirectory).
 *   4. If tif->tif_rawdata (tif+0x12c) != NULL and flag bit 0x40 set (tif+0xa),
 *      free it.
 *   5. __close(tif->tif_fd)  — tif+0x4 as int16 sign-extended.
 *   6. free(tif) itself.
 *
 * Struct offsets (derived from disassembly — no TIFF typedef in this project):
 *   tif+0x4  : tif_fd       (int16)   — file descriptor
 *   tif+0x6  : tif_flags    (int16)   — != 0 means flush needed
 *   tif+0xa  : flags byte — bit 0x40 = TIFF_MYBUFFER (owns raw data buffer)
 *   tif+0x11c: tif_cleanup  (fn ptr)  — called with tif as argument
 *   tif+0x12c: tif_rawdata  (void *)  — raw data buffer, freed if owned
 *
 * Call-site verification table:
 *   FUN_0006a260 call (TIFFFlush):
 *     arg1 | PUSH ESI (tif ptr) | tif | YES
 *   indirect call [ESI+0x11c]:
 *     EAX = [ESI+0x11c] (fn ptr, NULL-guarded)
 *     arg1 | PUSH ESI | tif | YES (CALL EAX)
 *   TIFFFreeDirectory call (TIFFFreeDirectory):
 *     arg1 | PUSH ESI | tif | YES
 *   debug_free calls:
 *     raw data: PUSH [ESI+0x12c], PUSH file_str, PUSH 0x37
 *     tif self: PUSH ESI, PUSH file_str, PUSH 0x3d */
void FUN_00064ee0(int tif_)
{
  char *tif = (char *)tif_;
  void (*cleanup_fn)(int);
  void *rawdata;

  /* Flush / write-back if flags indicate pending output. */
  if (*(short *)(tif + 0x6) != 0) {
    FUN_0006a260((void *)tif_);
  }

  /* Call per-codec cleanup callback if registered. */
  cleanup_fn = *(void (**)(int))(tif + 0x11c);
  if (cleanup_fn != (void (*)(int))0) {
    cleanup_fn(tif_);
  }

  /* Free directory data. */
  TIFFFreeDirectory(tif_);

  /* Free raw data buffer if owned by this TIFF object. */
  rawdata = *(void **)(tif + 0x12c);
  if (rawdata != 0 && (*(unsigned char *)(tif + 0xa) & 0x40) != 0) {
    debug_free(rawdata, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_close.c",
               0x37);
  }

  /* Close the underlying file descriptor. */
  __close((int)(*(short *)(tif + 0x4)));

  /* Free the TIFF object itself. */
  debug_free(tif, "c:\\halo\\SOURCE\\bitmaps\\libtiff\\tif_close.c", 0x3d);
}
