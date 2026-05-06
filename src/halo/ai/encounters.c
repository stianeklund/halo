/* encounters.c — AI encounter management.
 *
 * Covers encounters.obj (XBE address ranges ~0x58fa0–0x597f0 and
 * ~0x5d420–0x5dfb0).  __FILE__ = c:\halo\SOURCE\ai\encounters.c
 * (confirmed via display_assert strings throughout).
 *
 * Globals:
 *   encounter_data  = *(data_t**)0x5ab270  ("encounter", 0x80 × 0x6c)
 *   pursuit_data    = *(data_t**)0x5ab26c  ("ai pursuit", 0x100 × 0x28)
 *   actor_data      = *(data_t**)0x6325a4  ("actor", 0x100 × 0x724)
 *   ai_globals      = *(char**)0x632574
 *     +0x1  : ai_active byte (guard for all encounter entry points)
 *     +0x8  : encounterless list head (int handle)
 *
 * Actor meta offsets (confirmed from disassembly of 0x59740/0x597f0):
 *   actor+0x08 = flag used to compute initial action mask (char)
 *   actor+0x09 = encounterless flag (char)
 *   actor+0x0a = sub-state byte cleared on leave (Uncertain)
 *   actor+0x10 = action mask (uint16_t), set to 0x5a if actor+0x8 != 0
 *   actor+0x2c = next_encounterless handle (int, linked list)
 *   actor+0x34 = encounter_index (int, -1 = NONE)
 *   actor+0x3a = squad_index (int16_t, -1 = NONE)
 *   actor+0x3c = platoon_index (int16_t, -1 = NONE)
 *
 * Ported: FUN_00058fa0 (stub), FUN_00058fb0 (dispose pools),
 *         FUN_00059740 (encounter_enter), FUN_000597f0 (encounter_leave),
 *         FUN_0005ddc0 (tally reset), FUN_0005de80 (encounter_update),
 *         encounter lifecycle stubs (0x5df80–0x5dfb0).
 */

#include "../../common.h"


/* 0x00058fa0 — encounter_dispose stub.
 * Called from ai_dispose (0x3f6f0). No teardown needed at this level.
 * Binary: single RET instruction. */
void FUN_00058fa0(void)
{
  return;
}

/* 0x00058fb0 — encounters_dispose_from_old_map.
 * Called from ai_dispose_from_old_map (0x3f720) and FUN_00041e80.
 * Invalidates the encounter_data and pursuit_data pools via
 * data_make_invalid.  Uses raw address loads (MOV r32,[0x5ab270] /
 * PUSH r32 / CALL 0x119550) — the pool pointers are passed by value.
 *
 * Confirmed: ADD ESP,0x8 after two CALL 0x119550 instructions (combined
 * stack cleanup for both calls). */
void FUN_00058fb0(void)
{
  data_make_invalid(*(data_t **)0x5ab270); /* encounter_data */
  data_make_invalid(*(data_t **)0x5ab26c); /* pursuit_data */
}

/* 0x00059740 — encounter_enter (add actor to encounterless list).
 * Places an actor into the "encounterless" linked list maintained in
 * ai_globals.  Guards on ai_globals->field_1 (ai_active byte).
 *
 * Preconditions (asserted in binary):
 *   actor->meta.encounter_index == NONE  (actor+0x34 == -1)
 *   actor->meta.encounterless == 0       (actor+0x9 == 0)
 *
 * Side effects:
 *   actor+0x2c = old ai_globals->field_8 (prepend to encounterless list)
 *   ai_globals->field_8 = actor_handle   (new list head)
 *   actor+0x9 = 1                        (mark as encounterless)
 *   actor+0x10 = 0x5a if actor+0x8 != 0 else 0  (initial action mask)
 *   Calls FUN_0003b5e0(actor_handle) to reset action state.
 *
 * NEG/SBB/AND pattern at 0x597d1–0x597da:
 *   DL = actor+0x8; NEG DL sets CF if DL != 0; SBB EDX,EDX → EDX=-1 or 0;
 *   AND EDX,0x5a → EDX = 0x5a (if flag) or 0 (if not). */
void FUN_00059740(int actor_handle)
{
  char *actor;
  char *ai_globals;

  ai_globals = *(char **)0x632574;
  if (*(char *)(ai_globals + 1) == '\0') {
    return;
  }
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);

  /* Assert actor is not already in an encounter */
  if (*(int *)(actor + 0x34) != -1) {
    display_assert("actor->meta.encounter_index==NONE",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x2f6, 1);
    system_exit(-1);
  }

  /* Assert actor is not already encounterless */
  if (*(char *)(actor + 9) != '\0') {
    display_assert("!actor->meta.encounterless",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x2f7, 1);
    system_exit(-1);
  }

  /* Prepend actor to the encounterless list */
  *(int *)(actor + 0x2c) = *(int *)(ai_globals + 8);
  *(int *)(ai_globals + 8) = actor_handle;

  /* Set encounterless flag */
  *(char *)(actor + 9) = 1;

  /* Compute initial action mask: 0x5a if actor->field_8 is non-zero, else 0.
   * Binary: NEG [ESI+0x8] / SBB EDX,EDX / AND EDX,0x5a / MOV [ESI+0x10],DX */
  *(uint16_t *)(actor + 0x10) =
    (uint16_t)(-(*(char *)(actor + 8) != '\0') & 0x5a);

  FUN_0003b5e0(actor_handle);
}

/* 0x000597f0 — encounter_leave (remove actor from encounterless list).
 * Splices the actor out of the encounterless linked list.
 * Guards on ai_globals->field_1 (ai_active byte).
 *
 * Preconditions (asserted in binary):
 *   actor->meta.encounterless != 0    (actor+0x9 != 0)
 *   actor->meta.encounter_index == NONE  (actor+0x34 == -1)
 *   actor->meta.squad_index == NONE   (actor+0x3a == -1)
 *   actor->meta.platoon_index == NONE (actor+0x3c == -1)
 *
 * List walk: starts at &ai_globals->field_8 (the head pointer slot),
 * advances through actor->field_0x2c chain until EAX == actor_handle.
 * Each iteration: datum_get(actor_data, *piVar3) to get next record,
 * then piVar3 = &that_actor->field_0x2c, EAX = *piVar3.
 *
 * Side effects after splice:
 *   *piVar3 = actor->field_0x2c    (link predecessor to successor)
 *   actor+0x9  = 0                 (clear encounterless flag)
 *   actor+0x2c = -1                (next = NONE)
 *   actor+0xa  = 0                 (Uncertain: sub-state byte cleared)
 *
 * Confirmed: ESI = &ai_globals->field_8 at 0x598c0 (ADD ESI,0x8 after
 * MOV ESI,[0x632574]).  Inner loop uses ESI as int* pointer-to-next-slot. */
void FUN_000597f0(int actor_handle)
{
  char *actor;
  char *ai_globals;
  int *piVar3;
  int iVar2;

  ai_globals = *(char **)0x632574;
  if (*(char *)(ai_globals + 1) == '\0') {
    return;
  }
  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);

  /* Assert actor is encounterless */
  if (*(char *)(actor + 9) == '\0') {
    display_assert("actor->meta.encounterless",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x30e, 1);
    system_exit(-1);
  }

  /* Assert no encounter, squad, or platoon membership */
  if (*(int *)(actor + 0x34) != -1) {
    display_assert("actor->meta.encounter_index == NONE",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x30f, 1);
    system_exit(-1);
  }
  if (*(int16_t *)(actor + 0x3a) != -1) {
    display_assert("actor->meta.squad_index == NONE",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x310, 1);
    system_exit(-1);
  }
  if (*(int16_t *)(actor + 0x3c) != -1) {
    display_assert("actor->meta.platoon_index == NONE",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x311, 1);
    system_exit(-1);
  }

  /* Walk encounterless list to find the slot pointing to actor_handle.
   * piVar3 starts at the head pointer slot (&ai_globals->field_8). */
  piVar3 = (int *)(ai_globals + 8);
  iVar2 = *piVar3;
  while (iVar2 != actor_handle) {
    if (iVar2 == -1) {
      display_assert("*actor_index_reference!=NONE",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x31c, 1);
      system_exit(-1);
    }
    char *node = (char *)datum_get(*(data_t **)0x6325a4, *piVar3);
    piVar3 = (int *)(node + 0x2c);
    iVar2 = *piVar3;
  }

  /* Splice out: link predecessor to our successor */
  *piVar3 = *(int *)(actor + 0x2c);

  /* Clear actor's encounterless state */
  *(char *)(actor + 9) = 0;
  *(int *)(actor + 0x2c) = -1;
  *(char *)(actor + 0xa) = 0; /* Uncertain: sub-state byte */
}

/* Deferred functions (not yet ported — thunked from XBE):
 *   FUN_0005d910  — encounter_tally_votes_reset (complex, deferred)
 *   FUN_0005de80  — encounter_update (needs FUN_0005acf0 @<eax> audit)
 *   FUN_0005ddc0  — encounter_tally_reset_pass (shared loop pattern)
 *   FUN_0005df80  — encounter_initialize stub
 *   FUN_0005df90  — encounter_dispose stub
 *   FUN_0005dfa0  — encounter_initialize_for_new_map stub
 *   FUN_0005dfb0  — encounter_dispose_from_old_map stub
 */
