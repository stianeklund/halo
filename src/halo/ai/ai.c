/* ai.c — AI subsystem top-level lifecycle and query functions.
 *
 * Corresponds to ai.obj (XBE address range ~0x3f670–0x425b0).
 * Implements the dispose, dispose_from_old_map, place, and
 * enemies_can_see_player entry points. The initialize and
 * initialize_for_new_map functions are not yet ported.
 */

/* ai_dispose: shut down all AI subsystems in reverse-init order.
 * Calls eight subsidiary dispose functions and tail-calls the last one.
 * Confirmed: 7 CALL + 1 JMP (tail call to FUN_00048f50) in disassembly. */
void ai_dispose(void)
{
  FUN_00042b80();
  FUN_000540c0();
  FUN_00058fa0();
  FUN_00064140();
  actors_dispose();
  FUN_0005df90();
  FUN_00053640();
  FUN_00048f50();
}

/* ai_dispose_from_old_map: release per-map AI state when leaving a map.
 * Calls eight subsidiary dispose_from_old_map helpers, then clears the
 * AI active flag (byte at offset 1 in the AI globals struct) to mark the
 * subsystem inactive. Confirmed: 8 CALLs + MOV EAX,[0x632574] / MOV
 * byte ptr [EAX+1],0 in disassembly. */
void ai_dispose_from_old_map(void)
{
  FUN_00042ca0();
  FUN_000540e0();
  FUN_00058fb0();
  FUN_00064160();
  actors_dispose_from_old_map();
  FUN_0005dfb0();
  FUN_00053670();
  FUN_00048fa0();
  /* clear the AI active flag (offset 1 in the AI globals block) */
  *(char *)(*(int *)0x632574 + 1) = 0;
}

/* ai_place: JMP thunk — forwards directly to FUN_0005ddc0.
 * The binary at 0x3f760 is a single JMP instruction; the real body
 * lives at 0x5ddc0 (not yet identified as a named symbol). */
void ai_place(void)
{
  FUN_0005ddc0();
}

/* ai_enemies_can_see_player: query whether any AI enemy can currently see
 * a player. Delegates entirely to FUN_00042390(0). Returns true if any
 * enemy has line-of-sight to a player, false otherwise.
 * Confirmed: PUSH 0 / CALL 0x42390 / ADD ESP,4 / RET; caller (0xa74f0)
 * checks the return value as a bool. */
bool ai_enemies_can_see_player(void)
{
  return FUN_00042390(0);
}
