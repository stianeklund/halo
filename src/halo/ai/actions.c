/* actions.c — AI actor action dispatch.
 *
 * Corresponds to actions.obj.
 * Assertion path: c:\halo\SOURCE\ai\actions.c
 */

#include "../../common.h"

/* FUN_0001c300 (0x1c300) — actor_execute_current_action
 *
 * Dispatches the current action's execute handler via the action_definitions
 * table. Returns the handler's result, or 0 if no handler is set.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1c310.
 * Confirmed: actor+0x6c = action index (short), asserted in [0,14).
 * Confirmed: table at 0x253fb8, stride 0x38 (execute handler at +0x14 in
 * entry). Confirmed: handler called with (actor_handle), returns int32_t.
 * Confirmed: returns 0 when handler is NULL (XOR BL,BL; MOV AL,BL at 0x1c369).
 */
int32_t FUN_0001c300(int actor_handle)
{
  typedef int32_t (*action_execute_fn_t)(int);

  char *actor;
  short action;
  action_execute_fn_t handler;

  actor = (char *)datum_get(actor_data, actor_handle);
  action = *(short *)(actor + 0x6c);

  assert_halt(action >= 0 && action < 14);

  handler = *(action_execute_fn_t *)(0x253fb8 + action * 0x38);
  if (handler != NULL) {
    return handler(actor_handle);
  }
  return 0;
}

/* FUN_0001c450 (0x1c450)
 * Dispatch action-specific prop replacement for an actor.
 *
 * Looks up the actor via actor_data, validates the action index is in range,
 * then dispatches to the action's prop_replace handler (if any) through
 * the action_definitions table at 0x253fcc (stride 0x38, function pointer
 * at offset 0 of each entry).
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1c45f.
 * Confirmed: actor+0x6c is short action index.
 * Confirmed: assert at line 0xbe (190) checks 0 <= action < 14.
 * Confirmed: table at 0x253fcc, stride 0x38, first field is function pointer.
 * Confirmed: indirect call passes (actor_handle, old_prop, new_prop).
 * Confirmed: __FILE__ = "c:\halo\SOURCE\ai\actions.c". */
void FUN_0001c450(int actor_handle, int old_prop, int new_prop)
{
  typedef void (*action_prop_replace_fn_t)(int, int, int);

  char *actor;
  short action;
  action_prop_replace_fn_t handler;

  actor = (char *)datum_get(actor_data, actor_handle);
  action = *(short *)(actor + 0x6c);

  assert_halt(action >= 0 && action < 14);

  handler = *(action_prop_replace_fn_t *)(0x253fcc + action * 0x38);
  if (handler != NULL) {
    handler(actor_handle, old_prop, new_prop);
  }
}

/* FUN_0001d030 (0x1d030) — actor_set_action
 *
 * Transitions an actor to a new action type: calls the old action's exit
 * handler, adjusts the actor's priority level, copies action-specific state
 * data, sets the new action index, and calls the new action's begin handler.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x1d040.
 * Confirmed: INC word [0x5ac87c] (global action-change counter) at 0x1d04b.
 * Confirmed: assert new_action_type in [0,14) at line 0xb83.
 * Confirmed: assert table[new_action].action == new_action_type at line 0xb84.
 * Confirmed: assert actor+0x6c (old action) in [0,14) at line 0xb87.
 * Confirmed: exit handler at table+0x24 (0x253fc4) called with actor_handle.
 * Confirmed: priority adjust using table+0x10 (0x253fb0) short field.
 * Confirmed: FUN_00024b80(actor_handle, 0) at 0x1d122.
 * Confirmed: csmemcpy(actor+0x9c, param_3, data_size) when data_size > 0.
 * Confirmed: actor+0x6c = (short)param_2, actor+0x70 = 1 at 0x1d153/0x1d157.
 * Confirmed: begin handler at table+0x14 (0x253fb4) called with actor_handle.
 */
void FUN_0001d030(int actor_handle, int new_action_type, int param_3)
{
  typedef void (*action_handler_fn_t)(int);

  char *actor;
  int table_offset;
  action_handler_fn_t handler;

  actor = (char *)datum_get(actor_data, actor_handle);

  (*(uint16_t *)0x5ac87c)++;

  assert_halt(new_action_type >= 0 && new_action_type < 14);

  table_offset = new_action_type * 0x38;

  assert_halt(*(int *)(0x253fa0 + table_offset) == new_action_type);

  assert_halt(*(short *)(actor + 0x6c) >= 0 && *(short *)(actor + 0x6c) < 14);

  handler =
    *(action_handler_fn_t *)(0x253fc4 + *(short *)(actor + 0x6c) * 0x38);
  if (handler != NULL) {
    handler(actor_handle);
  }

  if (*(short *)(0x253fb0 + table_offset) == 0) {
    if (*(short *)(actor + 0x6a) > 2) {
      *(short *)(actor + 0x6a) = 2;
    }
  } else {
    if (*(short *)(actor + 0x6a) < 3) {
      *(short *)(actor + 0x6a) = 3;
    }
  }

  FUN_00024b80(actor_handle, 0);

  if (*(unsigned int *)(0x253fac + table_offset) != 0 && param_3 != 0) {
    csmemcpy(actor + 0x9c, (void *)param_3, *(int *)(0x253fac + table_offset));
  }

  *(short *)(actor + 0x6c) = (short)new_action_type;
  *(char *)(actor + 0x70) = 1;

  handler = *(action_handler_fn_t *)(0x253fb4 + (short)new_action_type * 0x38);
  if (handler != NULL) {
    handler(actor_handle);
  }
}
