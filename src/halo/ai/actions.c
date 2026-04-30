/* actions.c — AI actor action dispatch.
 *
 * Corresponds to actions.obj.
 * Assertion path: c:\halo\SOURCE\ai\actions.c
 */

#include "../../common.h"

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
