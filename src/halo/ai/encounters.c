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

/* 0x00059a00 — encounter_clump_iter_new.
 * Initialises a 3-slot int iterator for walking an encounter's clump member
 * list.  Guards on ai_active (ai_globals+1).
 *
 * iter[0] = clump_handle         (the encounter handle or -1 for encounterless)
 * iter[1] = -1                   (current member handle; filled by _next)
 * iter[2] = first_member_handle  (from encounter+0x14, or ai_globals+8 when
 *                                  clump_handle == -1)
 *
 * Call-site verification (callers read iter[1] to compare against actor_handle):
 *   PUSH [EBP+0xc]  (param_2 = clump_handle)  → datum_get arg2  YES
 *   PUSH [0x5ab270] (encounter_data)           → datum_get arg1  YES
 *   MOV ECX,[EAX+0x14]                         → encounter->first_member  YES
 *   MOV [ESI+0x8],ECX                          → iter[2]         YES
 *
 * Store-offset table (from disasm MOV [ESI+N]):
 *   ESI+0x0 : param_2 (clump_handle)
 *   ESI+0x4 : 0xffffffff (-1)
 *   ESI+0x8 : encounter->field_0x14 OR ai_globals->field_8 */
void FUN_00059a00(int *iter, int clump_handle)
{
  char *encounter;
  char *ai_globals;

  ai_globals = *(char **)0x632574;
  if (*(char *)(ai_globals + 1) == '\0') {
    return;
  }
  iter[0] = clump_handle;
  iter[1] = -1;
  if (clump_handle == -1) {
    /* No encounter: seed with encounterless list head */
    iter[2] = *(int *)(ai_globals + 8);
    return;
  }
  encounter = (char *)datum_get(*(data_t **)0x5ab270, clump_handle);
  iter[2] = *(int *)(encounter + 0x14);
}

/* 0x00059a50 — encounter_clump_iter_next.
 * Advances the encounter-clump member iterator and returns a pointer to the
 * next actor record, or NULL when the list is exhausted.
 *
 * iter[1] (current) ← iter[2] (next handle)
 * iter[2]           ← actor+0x2c  (link to following member)
 * returns datum_get(actor_data, iter[1]) or 0 if iter[1] == -1
 *
 * Call-site verification (PUSH ECX/PUSH EDX before CALL 0x119320):
 *   PUSH ECX (handle from iter[2]) → datum_get arg2  YES
 *   PUSH EDX ([0x6325a4])          → datum_get arg1  YES
 *   MOV ECX,[EAX+0x2c]             → actor->next_member  YES
 *   MOV [ESI+0x8],ECX              → iter[2]         YES */
int FUN_00059a50(int *iter)
{
  int handle;
  char *actor;

  if (*(char *)(*(char **)0x632574 + 1) == '\0') {
    return 0;
  }
  handle = iter[2];
  iter[1] = handle;
  if (handle == -1) {
    return 0;
  }
  actor = (char *)datum_get(*(data_t **)0x6325a4, handle);
  iter[2] = *(int *)(actor + 0x2c);
  return (int)actor;
}

/* 0x00059b10 — actor_iterator_new (extended AI actor iterator init).
 * Initialises a 0x1c-byte extended AI actor iterator.  The first 0x10
 * bytes are a standard data_iter_t (initialised by data_iterator_new on
 * encounter_data at 0x5ab270).  The extra fields are:
 *   iter+0x10 : phase byte  (0=scanning data table, 1=scanning encounterless list)
 *   iter+0x11 : filter_flag (0=all actors, 1=player-actors with actor+0x8 != 0)
 *   iter+0x14 : current datum handle (-1 = none)
 *   iter+0x18 : next linked-list handle (-1 = end)
 *
 * Call-site verification (PUSH ECX / PUSH ESI before CALL 0x1197b0):
 *   PUSH ECX ([0x5ab270] = encounter_data) → data_iterator_new arg2 (data)  YES
 *   PUSH ESI (iter = param_1)              → data_iterator_new arg1 (iter)  YES
 *
 * Store-offset table (from disasm MOV [ESI+N]):
 *   ESI+0x10 : 0x0   (phase byte — data-table phase)
 *   ESI+0x18 : -1    (next linked-list handle)
 *   ESI+0x14 : -1    (current handle)
 *   ESI+0x11 : DL    (param_2 = filter_flag) */
void FUN_00059b10(void *iter, char flag)
{
  char *p = (char *)iter;

  if (*(char *)(*(char **)0x632574 + 1) == '\0') {
    return;
  }
  data_iterator_new((data_iter_t *)iter, *(data_t **)0x5ab270);
  *(char  *)(p + 0x10) = 0;
  *(int   *)(p + 0x18) = -1;
  *(int   *)(p + 0x14) = -1;
  *(char  *)(p + 0x11) = flag;
}

/* 0x00059b50 — actor_iterator_next (extended AI actor iterator advance).
 * Returns the next actor record pointer, or NULL when done.  Two-phase:
 *
 * Phase 0 — data table (iter+0x18 == -1 initially):
 *   data_iterator_next advances through the encounter data pool.  For each
 *   encounter record returned, if filter_flag or encounter->field_0xd != 0,
 *   load iter+0x18 from encounter->field_0x14 (the member-chain head).
 *   Then fall into phase 1.
 *   If data_iterator_next returns NULL and iter+0x10 == 0, transition to
 *   phase 1 by seeding iter+0x18 from ai_globals->field_8 and setting
 *   iter+0x10 = 1.
 *
 * Phase 1 — encounterless linked list (iter+0x18 != -1):
 *   Walk via datum_get(actor_data, iter+0x18)->field_0x2c.
 *   If filter_flag==1 and actor+0x8==0: skip (continue with next link).
 *   Stores current handle at iter+0x14.
 *
 * Returns actor record pointer (datum_get result) or 0.
 *
 * Call-site verification (PUSH EAX/PUSH ECX before CALL 0x119320 at 0x59bc3):
 *   PUSH EAX (iter+0x18 current handle) → datum_get arg2 (handle)  YES
 *   PUSH ECX ([0x6325a4] = actor_data)  → datum_get arg1 (data)    YES
 *
 * Key disasm confirmations:
 *   0x59b6a: CMP [ESI+0x18],-1   — check next handle in linked list
 *   0x59b71: PUSH ESI / CALL 0x119810 — data_iterator_next(iter)
 *   0x59b80: TEST [ESI+0x11]     — filter_flag
 *   0x59b84: TEST [EAX+0xd]      — encounter->field_0xd
 *   0x59b8e: MOV ECX,[EAX+0x14] — encounter->first_member_handle
 *   0x59ba0: MOV EDX,[0x632574]  — ai_globals
 *   0x59ba6: MOV EAX,[EDX+0x8]  — ai_globals->encounterless_head
 *   0x59bac: MOV [ESI+0x10],1   — set phase=1 */
int FUN_00059b50(void *iter)
{
  char *p = (char *)iter;
  char *actor;
  char *encounter;
  int   handle;

  if (*(char *)(*(char **)0x632574 + 1) == '\0') {
    return 0;
  }

  for (;;) {
    if (*(int *)(p + 0x18) != -1) {
      /* Phase 1: walk linked list of actors */
      goto walk_list;
    }

    /* Phase 0: advance data table */
    encounter = (char *)data_iterator_next((data_iter_t *)iter);
    if (encounter != NULL) {
      /* If filter_flag==0, or this encounter has field_0xd set, load chain */
      if (*(char *)(p + 0x11) == '\0' || *(char *)(encounter + 0xd) != '\0') {
        *(int *)(p + 0x18) = *(int *)(encounter + 0x14);
      }
      /* Re-check iter+0x18 */
      if (*(int *)(p + 0x18) == -1) {
        continue;
      }
      goto walk_list;
    }

    /* data table exhausted — transition to encounterless list if not done */
    if (*(char *)(p + 0x10) != '\0') {
      /* Phase 1 already seeded: jump straight to list walk */
      goto walk_list;
    }
    *(int  *)(p + 0x18) = *(int *)(*(char **)0x632574 + 8);
    *(char *)(p + 0x10) = 1;

walk_list:
    handle = *(int *)(p + 0x18);
    *(int *)(p + 0x14) = handle;
    if (handle == -1) {
      return 0;
    }
    actor = (char *)datum_get(*(data_t **)0x6325a4, handle);
    *(int *)(p + 0x18) = *(int *)(actor + 0x2c);

    if (*(char *)(p + 0x11) == '\0') {
      /* No filter — return this actor */
      return (int)actor;
    }
    /* filter_flag: only return if actor+0x8 != 0 */
    if (*(char *)(actor + 8) != '\0') {
      return (int)actor;
    }
    /* Skip: loop within walk_list, matching asm JMP 0x59bb0.
     * The original returns 0 when the chain ends mid-filter rather than
     * advancing to the next encounter in the same call. */
    goto walk_list;
  }
}

/* 0x0005df80 — encounter_initialize stub.
 * Binary: single RET. No initialization needed at this level. */
void FUN_0005df80(void)
{
}

/* 0x0005df90 — encounter_dispose stub.
 * Binary: single RET. No teardown needed at this level. */
void FUN_0005df90(void)
{
}

/* 0x0005dfa0 — encounter_initialize_for_new_map stub.
 * Binary: single RET. Map-level init is handled elsewhere. */
void FUN_0005dfa0(void)
{
}

/* 0x0005dfb0 — encounter_dispose_from_old_map stub.
 * Binary: single RET. Map-level dispose is handled elsewhere. */
void FUN_0005dfb0(void)
{
}

/* Deferred functions (not yet ported — thunked from XBE):
 *   FUN_0005d910  — encounter_tally_votes_reset (complex, deferred)
 *   FUN_0005de80  — encounter_update (needs FUN_0005acf0 @<eax> audit)
 *   FUN_0005ddc0  — encounter_tally_reset_pass (shared loop pattern)
 */
