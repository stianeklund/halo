/* actors.c — AI actor/swarm data lifecycle.
 *
 * Corresponds to actors.obj (XBE address range ~0x3a990–0x3aab7).
 * Implements actors_dispose_from_old_map. Binary strings at
 * FUN_0003a990 confirm the source path "c:\halo\SOURCE\ai\actors.c"
 * and the three global data tables: actor_data (name "actor",
 * 0x100 entries * 0x724 bytes), swarm_data (name "swarm", 0x20 *
 * 0x98), and swarm_component_data (name "swarm component", 0x100 *
 * 0x40). actors_dispose (0x3aa50) is a single-RET stub in the
 * original binary — empty function, no operation.
 */

/* FUN_0003a990 (0x3a990)
 * Allocate actor_data, swarm_data, and swarm_component_data tables
 * via game_state_data_new. Asserts each allocation succeeded. */
void FUN_0003a990(void)
{
  FUN_0003a740();
  actor_data = game_state_data_new("actor", 0x100, 0x724);
  assert_halt(actor_data);
  swarm_data = game_state_data_new("swarm", 0x20, 0x98);
  assert_halt(swarm_data);
  swarm_component_data = game_state_data_new("swarm component", 0x100, 0x40);
  assert_halt(swarm_component_data);
}

/* actors_dispose: empty stub in the original binary (single RET).
 * Confirmed: 0x3aa50 contains only a RET instruction. */
void actors_dispose(void)
{
}

/* FUN_0003aa60 (0x3aa60)
 * Delete all entries from the three actor data tables. */
void FUN_0003aa60(void)
{
  data_delete_all(actor_data);
  data_delete_all(swarm_data);
  data_delete_all(swarm_component_data);
}

/* actors_dispose_from_old_map: invalidate the three actor data
 * tables when leaving a map. Calls data_make_invalid on
 * actor_data, swarm_data, swarm_component_data in that order.
 * Confirmed: three MOV r32,[global] / PUSH r32 / CALL 0x119550
 * sequences followed by ADD ESP,0xC / RET. */
void actors_dispose_from_old_map(void)
{
  data_make_invalid(actor_data);
  data_make_invalid(swarm_data);
  data_make_invalid(swarm_component_data);
}
