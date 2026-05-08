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
 * Ported: FUN_00058a40 (ai_magically_see_players), FUN_00058fa0 (stub),
 * FUN_00058fb0 (dispose pools), FUN_00059740 (encounter_enter), FUN_000597f0
 * (encounter_leave), FUN_0005ddc0 (tally reset), FUN_0005de80
 * (encounter_update), encounter lifecycle stubs (0x5df80–0x5dfb0).
 */

#include "../../common.h"


/* 0x00054020 — encounter_get_platoon_ptr (FUN_00054020).
 *
 * Returns a pointer to the platoon record for a given encounter and relative
 * platoon index. The platoon record lives in a flat array at *(char**)0x5ab274
 * with each element being 0x10 (16) bytes wide.
 *
 * Lookup process:
 *   1. Assert platoon_index is in [0, MAXIMUM_PLATOONS_PER_ENCOUNTER) and
 *      less than encounter->platoon_count (encounter+0xa).
 *   2. Compute absolute platoon index:
 *        platoon_absolute_index = encounter->platoon_start_index
 * (encounter+0x8)
 *                               + platoon_index
 *   3. Assert platoon_absolute_index is in [0, MAXIMUM_PLATOONS_PER_MAP).
 *   4. Return *(char**)0x5ab274 + platoon_absolute_index * 0x10.
 *
 * Constants (from assert strings + binary):
 *   MAXIMUM_PLATOONS_PER_ENCOUNTER = 0x20 (32)
 *   MAXIMUM_PLATOONS_PER_MAP       = 0x100 (256)
 *   platoon record size            = 0x10 (16 bytes)
 *
 * Source file: c:\halo\source\ai\encounters.h (inline, line 0xea / 0xed).
 * Inferred: the assert filepath is encounters.h, confirming this is an
 * inline helper compiled into encounters.obj. */
char *FUN_00054020(char *encounter, short platoon_index)
{
  short platoon_absolute_index;

  if (platoon_index < 0 || platoon_index >= 0x20 ||
      platoon_index >= *(short *)(encounter + 0xa)) {
    display_assert(
      "platoon_index>=0 && platoon_index<MAXIMUM_PLATOONS_PER_ENCOUNTER"
      " && platoon_index<encounter->platoon_count",
      "c:\\halo\\source\\ai\\encounters.h", 0xea, 1);
    system_exit(-1);
  }
  platoon_absolute_index = *(short *)(encounter + 0x8) + platoon_index;
  if (platoon_absolute_index < 0 || platoon_absolute_index >= 0x100) {
    display_assert("platoon_absolute_index>=0 && "
                   "platoon_absolute_index<MAXIMUM_PLATOONS_PER_MAP",
                   "c:\\halo\\source\\ai\\encounters.h", 0xed, 1);
    system_exit(-1);
  }
  return *(char **)0x5ab274 + (int)platoon_absolute_index * 0x10;
}


/* 0x00058a40 — ai_magically_see_players (FUN_00058a40).
 *
 * Forces all active players to be "magically seen" by the encounter
 * specified by combined_handle.  This overrides normal AI perception rules
 * so that every actor in the encounter immediately knows where all players
 * are, regardless of line-of-sight.
 *
 * Iff the AI trace flag at 0x5aca59 is non-zero, formats the encounter name
 * into a 256-byte stack buffer via FUN_00054220 then logs:
 *   "[scenario_tag_name]: ai_magically_see_players [encounter_name]"
 * via console_printf (channel 2).
 *
 * Then, iff combined_handle != -1, walks the player data pool
 * (*(data_t**)0x5aa6d4) using data_iterator_new / data_iterator_next and
 * calls FUN_00055110(combined_handle, player+0x34) for each live player.
 *
 * Confirmed:
 *   - ESI = param_1 throughout (callee-saved, loaded at 0x58a51).
 *   - global_scenario_get() at 0x58a62 takes 0 args; return in EAX.
 *   - Pre-push pattern: 0x100 and local_114 pushed before global_scenario_get
 *     for subsequent FUN_00054220 call; ADD ESP,0x10 at 0x58a74 cleans 4 args.
 *   - Pre-push pattern: local_114 pushed before FUN_000cb980 (0-arg) as 4th
 *     arg to console_printf; ADD ESP,0x10 at 0x58a8a cleans 4 dwords.
 *   - console_printf(2, fmt, cb980_result, name_buf): first %s = scenario
 *     tag name from FUN_000cb980, second %s = encounter name in name_buf.
 *   - MOV EDX,[0x005aa6d4] dereferences player_data before data_iterator_new.
 *   - player+0x34 is the field passed as arg2 to FUN_00055110.
 */
void FUN_00058a40(int combined_handle)
{
  char name_buf[256];
  char iter_buf[16];
  char *player;

  if (*(char *)0x5aca59 != '\0') {
    FUN_00054220((unsigned int)combined_handle, (void *)global_scenario_get(),
                 name_buf, 0x100);
    console_printf(2, "%s: ai_magically_see_players %s",
                   (const char *)FUN_000cb980(), name_buf);
  }
  if (combined_handle != -1) {
    data_iterator_new((data_iter_t *)iter_buf, *(data_t **)0x5aa6d4);
    player = (char *)data_iterator_next((data_iter_t *)iter_buf);
    while (player != (char *)0) {
      FUN_00055110(combined_handle, *(int *)(player + 0x34));
      player = (char *)data_iterator_next((data_iter_t *)iter_buf);
    }
  }
}

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

/* 0x59480 — Remove actor from encounter membership.
 * Unlinks the actor from the encounter's member linked list (rooted at
 * encounter+0x14, chained via actor+0x2c). When flag==0, also decrements
 * original counts on encounter, squad, leader, and platoon. Clears the
 * actor's encounter/squad/platoon fields and marks the encounter dirty. */
void FUN_00059480(int actor_handle, char flag)
{
  char *actor;
  char *encounter;
  int *piVar;
  int iVar;
  char *squad;
  char *platoon;

  if (*(char *)(*(char **)0x632574 + 1) == '\0')
    return;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(int *)(actor + 0x34) == -1)
    return;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, *(int *)(actor + 0x34));
  piVar = (int *)(encounter + 0x14);
  iVar = *(int *)(encounter + 0x14);
  while (iVar != actor_handle) {
    if (iVar == -1) {
      display_assert("*actor_index_reference!=NONE",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x220, 1);
      system_exit(-1);
    }
    iVar = (int)datum_get(*(data_t **)0x6325a4, *piVar);
    piVar = (int *)(iVar + 0x2c);
    iVar = *piVar;
  }
  *piVar = *(int *)(actor + 0x2c);

  if (flag == '\0') {
    squad = FUN_0001c270(encounter, *(int16_t *)(actor + 0x3a));

    if (*(int16_t *)(encounter + 0x18) < 1) {
      display_assert("encounter->original_count > 0",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x22c, 1);
      system_exit(-1);
    }
    (*(int16_t *)(encounter + 0x18))--;

    if (*(char *)(actor + 0x1c) != '\0') {
      if (*(int16_t *)(encounter + 0x1c) < 1) {
        display_assert("encounter->unique_leader_count > 0",
                       "c:\\halo\\SOURCE\\ai\\encounters.c", 0x231, 1);
        system_exit(-1);
      }
      (*(int16_t *)(encounter + 0x1c))--;
    }

    if (*(int16_t *)(squad + 0x16) < 1) {
      display_assert("squad->original_count > 0",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x235, 1);
      system_exit(-1);
    }
    (*(int16_t *)(squad + 0x16))--;

    if (*(int16_t *)(actor + 0x3c) != -1) {
      platoon = FUN_00054020(encounter, *(int16_t *)(actor + 0x3c));
      if (*(int16_t *)(platoon + 4) < 1) {
        display_assert("platoon->original_count > 0",
                       "c:\\halo\\SOURCE\\ai\\encounters.c", 0x23d, 1);
        system_exit(-1);
      }
      (*(int16_t *)(platoon + 4))--;
    }
  }

  *(int *)(actor + 0x2c) = -1;
  *(int *)(actor + 0x34) = -1;
  *(int16_t *)(actor + 0x3c) = -1;
  *(int16_t *)(actor + 0x3a) = -1;
  *(char *)(encounter + 0x28) = 1;
}

/* 0x59630 — Validate encounter/actor linkage for a unit and optionally set
 * the encounter's team index from the unit. If the unit has a secondary actor
 * (field 0x1a8), validates that actor's encounter matches. Otherwise validates
 * the primary actor (field 0x1a4) encounter and unit linkage. When
 * game_engine_running returns false and encounter->team is zero, copies the
 * unit's team and notifies via FUN_00040280 if members exist. */
void FUN_00059630(int encounter_index, int unit_index)
{
  char *encounter;
  char *unit;
  char *actor;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_index);
  unit = (char *)object_get_and_verify_type(unit_index, 3);

  if (*(int *)(unit + 0x1a8) != -1) {
    actor = (char *)datum_get(*(data_t **)0x6325a4, *(int *)(unit + 0x1a8));
    if (*(int *)(actor + 0x34) != encounter_index) {
      display_assert("actor->meta.encounter_index == encounter_index",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x25b, 1);
      system_exit(-1);
    }
  } else {
    actor = (char *)datum_get(*(data_t **)0x6325a4, *(int *)(unit + 0x1a4));
    if (*(int *)(actor + 0x34) != encounter_index) {
      display_assert("actor->meta.encounter_index == encounter_index",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x261, 1);
      system_exit(-1);
    }
    if (*(int *)(actor + 0x18) != unit_index) {
      display_assert("actor->meta.unit_index == unit_index",
                     "c:\\halo\\SOURCE\\ai\\encounters.c", 0x262, 1);
      system_exit(-1);
    }
  }

  if (!game_engine_running() && *(int16_t *)(encounter + 2) == 0) {
    *(int16_t *)(encounter + 2) = *(int16_t *)(unit + 0x68);
    if (*(int *)(encounter + 0x14) != -1)
      FUN_00040280();
  }
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
  char *node;
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
    node = (char *)datum_get(*(data_t **)0x6325a4, *piVar3);
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
 * Call-site verification (callers read iter[1] to compare against
 * actor_handle): PUSH [EBP+0xc]  (param_2 = clump_handle)  → datum_get arg2 YES
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
 *   iter+0x10 : phase byte  (0=scanning data table, 1=scanning encounterless
 * list) iter+0x11 : filter_flag (0=all actors, 1=player-actors with actor+0x8
 * != 0) iter+0x14 : current datum handle (-1 = none) iter+0x18 : next
 * linked-list handle (-1 = end)
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
  *(char *)(p + 0x10) = 0;
  *(int *)(p + 0x18) = -1;
  *(int *)(p + 0x14) = -1;
  *(char *)(p + 0x11) = flag;
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
  int handle;

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
    *(int *)(p + 0x18) = *(int *)(*(char **)0x632574 + 8);
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

/* 0x5adc0 — encounter_squad_delay_timer_finished.
 * Called when a squad's delay timer expires (count < 0x10 ticks).
 * Resets the squad's delay counter to 0, then optionally triggers
 * ai_magically_see_players on the squad (if squad_def flag 0x10 is set),
 * and logs a debug message if the debug flag (0x5aca4b) is set.
 *
 * param_1 = encounter_handle (int, full datum handle)
 * param_2 = squad_index (int16_t, index within encounter)
 *
 * Confirmed:
 *   - 2 cdecl stack args; ADD ESP,0x8 at callers (0x1cb00, 0x5af13, 0x5af0e).
 *   - datum_get(*(data_t**)0x5ab270, encounter_handle) → ESI=encounter record.
 *   - AND EDI,0xffff at 0x5add8 → encounter_handle masked before scenario
 * lookup.
 *   - global_scenario_get() at 0x5ade6 (0 args); +0x42c+[EDI] element (size
 * 0xb0).
 *   - FUN_0001c270(encounter, param_2) → squad record; EBX=squad pointer.
 *   - tag_block_get_element(EBX+0x80, (int16_t)param_2, 0xe8) → squad_def.
 *   - MOV word ptr [ECX+0x12],0 at 0x5ae1e clears squad delay counter.
 *   - Bit 0x10 of squad_def+0x28 gates FUN_00058a40 call
 * (ai_magically_see_players).
 *   - Handle for FUN_00058a40: ((squad_index & 0xff | 0xffff8000) << 16) |
 * (encounter_handle & 0xffff).
 *   - ADD ESP,0x20 at 0x5ae27 cleans up 8 dwords (first tag_block 3 +
 * FUN_0001c270 2 + second tag_block 3).
 *   - ADD ESP,0x4 at 0x5ae4c cleans FUN_00058a40 arg.
 *   - ADD ESP,0x10 at 0x5ae67 cleans console_printf args.
 *
 * Store-offset table (squad record writes):
 *   squad_record+0x12 | 0 (int16_t zero) | MOV word ptr [ECX+0x12],0x0 at
 * 0x5ae1e
 */
void FUN_0005adc0(int encounter_handle, int16_t squad_index)
{
  char *encounter;
  char *squad;
  char *squad_record;
  char *squad_def;
  int handle;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  squad = (char *)tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                        (int)(encounter_handle & 0xffff), 0xb0);
  squad_record = (char *)FUN_0001c270(encounter, squad_index);
  squad_def = (char *)tag_block_get_element((char *)(squad + 0x80),
                                            (int)squad_index, 0xe8);
  *(int16_t *)(squad_record + 0x12) = 0;
  if ((*(unsigned char *)(squad_def + 0x28) & 0x10) != 0) {
    handle =
      (int)(((unsigned int)(((int)squad_index & 0xff) | 0xffff8000U) << 16) |
            (unsigned int)(encounter_handle & 0xffff));
    FUN_00058a40(handle);
  }
  if (*(char *)0x5aca4b != '\0') {
    console_printf(0, "%s/%s: delay timer finished", squad, squad_def);
  }
}

/* 0x5ae70 — encounter_update_squad_delay_timers.
 * Iterates all squads in an encounter and manages their delay timers.
 * For each squad with a positive delay counter (squad+0x12):
 *   - If the delay is not yet started (squad+0x11 == 0): starts the timer
 *     when either bit 2 of squad_def+0x28 is set OR encounter+0x2e > 0.
 *     Logs "%s/%s: delay timer started (%.1f sec)" when debug flag is set.
 *   - If the delay is running and >= 0x10 ticks: decrements by 15.
 *   - If the delay is running and < 0x10 ticks: fires FUN_0005adc0 to
 *     complete the squad spawn.
 * Squads with bit 3 of squad_def+0x28 set are skipped entirely.
 *
 * Confirmed: PUSH [EBP+8] before CALL 0x5ae70 in FUN_0005de80 (0x5df42).
 * Confirmed: ADD ESP,0xc after global_scenario_get + tag_block_get_element
 *   (pre-positioned args pattern: 0xb0, ESI pushed before global_scenario_get).
 * Confirmed: FUN_0005adc0(encounter_handle, squad_index) — 2 cdecl args.
 * Confirmed: float at iVar5+0x50 promoted to double via FSTP [ESP]. */
void FUN_0005ae70(int encounter_handle)
{
  char *encounter;
  char *scenario;
  char *squad;
  char *squad_def;
  int16_t squad_count;
  int16_t squad_index;
  int16_t delay;
  char started;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  scenario = (char *)global_scenario_get();
  squad = (char *)tag_block_get_element(
    (char *)(scenario + 0x42c), (int)(int16_t)(encounter_handle & 0xffff),
    0xb0);
  squad_index = 0;
  squad_count = *(int16_t *)(encounter + 0x6);
  if (squad_count <= 0) {
    return;
  }
  do {
    char *sq = (char *)FUN_0001c270(encounter, squad_index);
    squad_def = (char *)tag_block_get_element((char *)(squad + 0x80),
                                              (int)squad_index, 0xe8);
    delay = *(int16_t *)(sq + 0x12);
    if (delay > 0 && (*(unsigned int *)(squad_def + 0x28) & 8) == 0) {
      if (*(char *)(sq + 0x11) == '\0') {
        if ((*(unsigned int *)(squad_def + 0x28) & 4) == 0 &&
            *(int16_t *)(encounter + 0x2e) < 1) {
          started = '\0';
        } else {
          started = '\x01';
        }
        *(char *)(sq + 0x11) = started;
        if (started != '\0' && *(char *)0x5aca4b != '\0') {
          console_printf(0, "%s/%s: delay timer started (%.1f sec)", squad,
                         squad_def, (double)*(float *)(squad_def + 0x50));
        }
      } else if (delay < 0x10) {
        FUN_0005adc0(encounter_handle, squad_index);
      } else {
        *(int16_t *)(sq + 0x12) = delay - 15;
      }
    }
    squad_index = squad_index + 1;
  } while (squad_index < *(int16_t *)(encounter + 0x6));
}

/* 0x5b200 — encounters_initialize_for_new_map.
 * Resets encounter and pursuit data pools, zeroes squad and platoon arrays,
 * then iterates scenario encounter definitions calling FUN_0005a120 to
 * initialize each encounter record. */
void FUN_0005b200(void)
{
  char *scenario;
  short i;
  short squad_counter;
  short platoon_counter;
  void *encounter_def;

  scenario = (char *)global_scenario_get();
  squad_counter = 0;
  platoon_counter = 0;
  data_delete_all(*(data_t **)0x5ab270);
  data_delete_all(*(data_t **)0x5ab26c);
  csmemset(*(void **)0x5ab278, 0, 0x8000);
  csmemset(*(void **)0x5ab274, 0, 0x1000);
  if (*(int *)(scenario + 0x42c) > 0) {
    for (i = 0; (int)i < *(int *)(scenario + 0x42c); i++) {
      encounter_def =
        tag_block_get_element((void *)(scenario + 0x42c), (int)i, 0xb0);
      FUN_0005a120(&squad_counter /* @<eax> */, encounter_def,
                   &platoon_counter);
    }
  }
}

/* 0x5c940 — encounter_update_platoon_rules.
 * For each platoon in the encounter, evaluates two rule conditions:
 *   1. Maneuvering rule (platoon_def+0x3c): if platoon[1]==0 (not yet set),
 *      calls FUN_0005af70 to evaluate; stores result in platoon[1].
 *   2. Defend/attack rule (platoon_def+0x30): if platoon[2]!=0 or
 * platoon[1]==0, computes the expected attacking flag from platoon_def+0x20 bit
 * 2 (inverted), and if it differs from platoon[0], calls FUN_0005af70 to
 * confirm, then sets platoon[0] to the new value. Logs state transitions via
 * console_printf when DAT_005aca4b is set.
 *
 * Confirmed:
 *   - cdecl, 1 stack arg (encounter_handle); no ADD ESP after RET.
 *   - datum_get(*(data_t**)0x5ab270, encounter_handle) → encounter record.
 *   - PUSH 0xb0 / PUSH ESI / CALL global_scenario_get / ADD EAX,0x42c / PUSH
 * EAX / CALL tag_block_get_element / ADD ESP,0xc → encounter def element at
 *     global_scenario_get()+0x42c[encounter_handle&0xffff] with
 * element_size=0xb0.
 *   - Outer loop on platoon_count = *(int16_t*)(encounter+0xa), via
 * FUN_00054020.
 *   - Inner tag_block_get_element: enc_def+0x8c block, loop_index,
 * element_size=0xac.
 *   - First FUN_0005af70 call: EAX=encounter_handle, EDI=platoon_def+0x3c.
 *   - Second FUN_0005af70 call: EAX=encounter_handle, EDI=platoon_def+0x30.
 *   - BL = ~(*(uint32_t*)(platoon_def+0x20) >> 2) & 1 (attacking flag).
 *   - Loop counter stored/restored via [EBP-0xc] / DI (16-bit); EBX=[EBP-0x10]
 *     (encounter) restored at bottom; ESI=platoon record from FUN_00054020.
 *
 * Call-site verification (FUN_0005de80 @ 0x5df4b):
 *   arg1 | PUSH EAX ([EBP-0x8] = encounter_handle) | encounter_handle | YES
 *
 * Store-offset table (platoon record writes):
 *   platoon[0] — attacking flag (byte), written at 0x5ca2f from BL
 *   platoon[1] — maneuvering enabled (byte), written at 0x5c9dc from AL
 * (FUN_0005af70 result)
 */
void FUN_0005c940(int encounter_handle)
{
  char *encounter;
  char *enc_def_elt;
  char *platoon;
  char *platoon_def;
  unsigned int flags;
  char new_flag;
  char result;
  const char *label;
  int i;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
  enc_def_elt = (char *)tag_block_get_element(
    (char *)global_scenario_get() + 0x42c,
    (int)(short)(encounter_handle & 0xffff), 0xb0);
  i = 0;
  if (*(short *)(encounter + 0xa) <= 0) {
    return;
  }
  do {
    platoon = (char *)FUN_00054020(encounter, (short)i);
    if (*(short *)(platoon + 6) > 0) {
      platoon_def =
        (char *)tag_block_get_element(enc_def_elt + 0x8c, (int)(short)i, 0xac);

      /* Maneuvering rule: evaluate once (platoon[1] is the latch). */
      if (platoon[1] == '\0') {
        result = (char)FUN_0005af70(encounter_handle /* @<eax> */,
                                    platoon_def + 0x3c /* @<edi> */);
        platoon[1] = result;
        if (result != '\0' && *(char *)0x5aca4b != '\0') {
          console_printf(0, "%s/%s triggered maneuvering rule", enc_def_elt,
                         platoon_def);
        }
      }

      /* Defend/attack rule: re-evaluate when platoon[2]!=0 or platoon[1]==0. */
      if (platoon[2] != '\0' || platoon[1] == '\0') {
        flags = *(unsigned int *)(platoon_def + 0x20);
        new_flag = (char)(~(flags >> 2) & 1u);
        if (platoon[0] != new_flag) {
          result = (char)FUN_0005af70(encounter_handle /* @<eax> */,
                                      platoon_def + 0x30 /* @<edi> */);
          if (result != '\0') {
            platoon[0] = new_flag;
            if (*(char *)0x5aca4b != '\0') {
              label = "defending";
              if (new_flag == '\0') {
                label = "attacking";
              }
              console_printf(0, "%s/%s triggered %s rule", enc_def_elt,
                             platoon_def, label);
            }
          }
        }
      }
    }
    i = i + 1;
  } while ((short)i < *(short *)(encounter + 0xa));
}

/* 0x5d200 — Add actor to encounter/squad/platoon membership.
 *
 * Assigns an actor to a specific encounter, squad, and platoon slot.
 * Updates all linked-list bookkeeping (encounter member list, squad count,
 * platoon count, leader count), optionally changes the actor's team, and
 * activates/deactivates the actor via encounter state flags.
 *
 * Preconditions (asserted in binary):
 *   actor->field_0x34 == -1  (actor not already in an encounter)
 *
 * Parameters:
 *   actor_handle    — datum index of the actor to add
 *   encounter_index — datum index of the target encounter
 *   squad_index     — squad slot within the encounter (int16_t)
 *   flag            — non-zero: force team assignment (suppress actor-team
 * change; if encounter->field_0x2a != 0, warns and forces anyway)
 *
 * Side effects:
 *   actor->field_0x30  = -1          (clear meta field)
 *   actor->field_0x38  = 0xffff      (clear meta short)
 *   actor->field_0x2c  = old encounter->field_0x14  (prepend to member list)
 *   encounter->field_0x14 = actor_handle
 *   actor->field_0x3c  = platoon_index (or -1 if out of range)
 *   actor->field_0x34  = encounter_index
 *   actor->field_0x3a  = squad_index
 *   encounter->field_0xe = 0x96 if actor->field_0x8 && !actor->field_0x13
 *   encounter->field_0x28 = 1  (dirty flag)
 *
 * Confirmed: 4 cdecl args, ADD ESP,0x10 at caller 0x3bb04.
 * Confirmed: assert string "actor->meta.encounter_index==NONE" at 0x5d2bd,
 *   file "c:\halo\SOURCE\ai\encounters.c", line 0x28a.
 * Confirmed: FUN_0005a4e0 reads encounter_index from EAX (@<eax> register arg).
 * Confirmed: EBX=encounter_index preserved across all inner calls.
 * Confirmed: ESI=actor record, EDI=encounter record throughout.
 */
void FUN_0005d200(int actor_handle, int encounter_index, int16_t squad_index,
                  int flag)
{
  char *actor;
  char *encounter;
  char *enc_def;
  char *squad_def;
  char *squad;
  char *platoon;
  int platoon_index;
  short platoon_index_s;
  int unit_index;

  if (*(char *)(*(char **)0x632574 + 1) == '\0')
    return;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  tag_get(0x61637472,
          *(int *)(actor + 0x58)); /* 'actr' tag — result discarded */
  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_index);

  /* Build enc_def: scenario encounter block element */
  enc_def =
    (char *)tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                  (int)(encounter_index & 0xffff), 0xb0);

  /* Squad record from encounter runtime data */
  squad = (char *)FUN_0001c270(encounter, (int)squad_index);

  /* Squad definition element from tag block */
  squad_def = (char *)tag_block_get_element(enc_def + 0x80,
                                            (int)(int16_t)squad_index, 0xe8);

  /* platoon_index from squad def field +0x22 */
  platoon_index_s = *(short *)(squad_def + 0x22);
  platoon_index = (int)platoon_index_s;

  /* Assert actor is not already in an encounter */
  if (*(int *)(actor + 0x34) != -1) {
    display_assert("actor->meta.encounter_index==NONE",
                   "c:\\halo\\SOURCE\\ai\\encounters.c", 0x28a, 1);
    system_exit(-1);
  }

  /* Clear actor meta fields */
  *(int *)(actor + 0x30) = -1;
  *(short *)(actor + 0x38) = (short)-1; /* 0xffff */

  /* Prepend actor to encounter member list (rooted at encounter+0x14) */
  *(int *)(actor + 0x2c) = *(int *)(encounter + 0x14);
  *(int *)(encounter + 0x14) = actor_handle;

  /* Clamp platoon_index: validate against enc_def->field_0x8c count */
  if (platoon_index_s < 0 || platoon_index >= *(int *)(enc_def + 0x8c)) {
    platoon_index = -1;
    platoon_index_s = -1;
  }

  /* Store encounter membership fields on actor */
  *(short *)(actor + 0x3c) = (short)platoon_index; /* platoon_index */
  *(int *)(actor + 0x34) = encounter_index;
  *(short *)(actor + 0x3a) = squad_index;

  /* If actor is active (field_0x8) and not already activated (field_0x13):
   * mark encounter as pending (field_0xe = 0x96) and check activation. */
  if (*(char *)(actor + 0x8) != '\0' && *(char *)(actor + 0x13) == '\0') {
    /* Re-fetch encounter record for the field_0xe write */
    encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_index);
    *(short *)(encounter + 0xe) = 0x96;
    if ((char)FUN_0005a4e0(encounter_index /* @<eax> */) != '\0')
      goto LAB_0005d365;
  }

  FUN_0003d5f0(actor_handle, *(char *)(encounter + 0xd));
  if (*(char *)(encounter + 0xd) != '\0')
    FUN_0003ca40(actor_handle, 0);

LAB_0005d365:
  /* If actor has a unit, validate linkage */
  unit_index = *(int *)(actor + 0x18);
  if (unit_index != -1)
    FUN_00059630(encounter_index, unit_index);

  /* Team change logic */
  if (*(short *)(actor + 0x3e) != *(short *)(encounter + 2)) {
    if (flag == 0) {
      /* Actor drives its own team assignment */
      FUN_0003aac0(actor_handle,
                   (int)(unsigned short)*(short *)(encounter + 2));
    } else if (*(short *)(encounter + 0x2a) == 0) {
      /* Encounter has no fixed team: adopt actor's team */
      *(short *)(encounter + 2) = *(short *)(actor + 0x3e);
      FUN_00040280();
    } else {
      /* Team conflict: warn, then force actor to encounter team */
      console_printf(
        2,
        "WARNING: actor changing to encounter %s/%s is being forced "
        "to change teams",
        enc_def, squad_def);
      FUN_0003aac0(actor_handle,
                   (int)(unsigned short)*(short *)(encounter + 2));
    }
  }

  /* Increment actor count on encounter and squad */
  *(short *)(encounter + 0x18) = *(short *)(encounter + 0x18) + 1;
  *(short *)(squad + 0x16) = *(short *)(squad + 0x16) + 1;

  /* Increment leader count if actor is a leader */
  if (*(char *)(actor + 0x1c) != '\0')
    *(short *)(encounter + 0x1c) = *(short *)(encounter + 0x1c) + 1;

  /* Platoon membership */
  if ((short)platoon_index != -1) {
    platoon = (char *)FUN_00054020(encounter, platoon_index);
    *(char *)(actor + 0x1c9) = *platoon;
    *(char *)(actor + 0x374) = *platoon;
    *(short *)(platoon + 4) = *(short *)(platoon + 4) + 1;
  }

  /* Mark encounter dirty */
  *(char *)(encounter + 0x28) = 1;
}

/* 0x5d890 — Iterate all encounters; for each dirty encounter whose
 * flag at +0x28 is set, calls FUN_0005d420 (encounter_finalize/recycle).
 *
 * Uses the same guarded iterator pattern as FUN_0005ddc0:
 *   - Guards on ai_globals+1 before init and at every loop iteration.
 *   - Inner do-while skips encounters whose dirty flag (+0xd) is clear,
 *     unless flag==0 (first call after init, which forces one iteration).
 *   - encounter_handle is read from iter.datum_handle (EBP-0x10 in
 *     disassembly, confirmed offset 0x08 of data_iter_t).
 *   - Decompiler aliasing bug: showed local_14 (EBP-0x14) for the handle;
 *     disassembly clearly reads EBP-0x10 = iter.datum_handle.
 *
 * Call-site verification (PUSH ECX at 0x5d8ff → CALL 0x5d420):
 *   arg1 | EBP-0x10 = iter.datum_handle | encounter_handle | match YES
 *
 * Store-offset table (none: no struct init, only reads):
 *   encounter+0x0d — dirty flag, read in inner loop continue condition
 *   encounter+0x28 — recycle-pending flag, gates FUN_0005d420 call
 */
void FUN_0005d890(void)
{
  data_iter_t iter;
  int encounter_handle;
  char *encounter;
  char flag;

  if (*(char *)(*(int *)0x632574 + 1) != '\0') {
    data_iterator_new(&iter, *(data_t **)0x5ab270);
    flag = '\0';
  }
  for (;;) {
    if (*(char *)(*(int *)0x632574 + 1) == '\0')
      return;
    do {
      encounter = (char *)data_iterator_next(&iter);
      if (encounter == NULL || flag == '\0')
        break;
    } while (*(char *)(encounter + 0xd) == '\0');
    encounter_handle = (int)iter.datum_handle;
    if (encounter == NULL)
      return;
    if (*(char *)(encounter + 0x28) != '\0') {
      FUN_0005d420(encounter_handle);
    }
  }
}

/* 0x0005d910 — Place actors for an encounter or specific squad/platoon.
 * param_2: platoon index (-1 = all), param_3: squad index (-1 = all).
 * Resolves difficulty-based spawn counts, applies spawn-type delays,
 * calls FUN_0005c3a0 per actor slot, then finalises via FUN_0005d420 and
 * FUN_0005a6e0. */
void FUN_0005d910(int encounter_handle, short param_2, short param_3)
{
  char *scenario;
  char *encounter_def;
  char *squad_def;
  char *name;
  char *encounter;
  char do_all;
  int i;
  int count;
  int delay;
  int should_spawn;
  int leader_count;
  int squad_state;
  int spawn_type;
  int j;
  int16_t difficulty;

  if (*(char *)(*(int *)0x632574 + 1) == 0) {
    return;
  }

  scenario = (char *)global_scenario_get();
  encounter_def = (char *)tag_block_get_element(
    (void *)(scenario + 0x42c), encounter_handle & 0xffff, 0xb0);
  do_all = (param_2 == -1 && param_3 == -1);

  if (*(char *)0x5aca52) {
    if (param_2 == -1 && param_3 == -1) {
      console_printf(0, "ai_place %s", encounter_def);
    } else if (param_2 != -1) {
      name = (char *)tag_block_get_element((void *)(encounter_def + 0x8c),
                                           param_2, 0xac);
      console_printf(0, "ai_place %s/%s", encounter_def, name);
    } else {
      name = (char *)tag_block_get_element((void *)(encounter_def + 0x80),
                                           param_3, 0xe8);
      console_printf(0, "ai_place %s/%s", encounter_def, name);
    }
  }

  for (i = 0; (int)(int16_t)i < *(int *)(encounter_def + 0x80); i++) {
    squad_def = (char *)tag_block_get_element((void *)(encounter_def + 0x80),
                                              (int16_t)i, 0xe8);

    if (!do_all && (int16_t)i != (int16_t)param_3 &&
        (*(int16_t *)(squad_def + 0x22) == -1 ||
         *(int16_t *)(squad_def + 0x22) != (int16_t)param_2)) {
      continue;
    }

    delay = 0;

    difficulty = game_difficulty_level_get();
    switch (difficulty) {
    case 0:
    case 1:
      count = *(uint16_t *)(squad_def + 0x7c);
      break;
    case 2:
      count = ((int16_t)(*(int16_t *)(squad_def + 0x7e) +
                         *(int16_t *)(squad_def + 0x7c))) /
              2;
      break;
    case 3:
      count = *(uint16_t *)(squad_def + 0x7e);
      break;
    default:
      assert_halt(0);
      break;
    }

    squad_state = (int)FUN_0005a3b0((void *)squad_def);
    spawn_type = *(int16_t *)(squad_def + 0x2c);

    switch (spawn_type) {
    case 0:
      encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);
      should_spawn = 0;
      leader_count = 0;
      if (squad_state == 7) {
        leader_count = (int)*(int16_t *)(encounter + 0x1c);
        if (leader_count == 0) {
          should_spawn = (*(int16_t *)(encounter + 0x18) + (int16_t)count >= 4);
        } else if (leader_count == 1) {
          should_spawn =
            (*(int16_t *)(encounter + 0x18) + (int16_t)count >= 10);
        }
      }
      if (*(char *)0x5aca52) {
        console_printf(
          0, "%s/%s: %d current %d leaders, create %d -> %s", encounter_def,
          squad_def, (int)*(int16_t *)(encounter + 0x18), leader_count,
          (int16_t)count, should_spawn ? "new leader" : "no leader");
      }
      if (!should_spawn) {
        break;
      }
      /* fall through to case 2 */
    case 2:
      if (squad_state == 7) {
        delay = (int)random_range(
                  (unsigned int *)get_global_random_seed_address(), 0, 2) +
                100;
      }
      break;
    case 3:
      if (squad_state == 7) {
        delay = 100;
      }
      break;
    case 4:
      if (squad_state == 7) {
        delay = 101;
      }
      break;
    default:
      break;
    }

    if ((int16_t)count > 0) {
      for (j = 0; j < (int16_t)count; j++) {
        FUN_0005c3a0((int16_t)i, delay, 0, encounter_handle);
        delay = 0;
      }
    }
  }

  FUN_0005d420(encounter_handle);
  FUN_0005a6e0();
}

/* 0x0005dc00 — Sync actor state from encounter and platoon definitions.
 *
 * For a given encounter handle, walks the linked list of actors belonging to
 * that encounter (rooted at encounter+0x14, chained via actor+0x2c).
 * For each actor the function:
 *   - Copies encounter-level flag bytes into the actor record
 *     (encounter+0x42 -> actor+0x1c8, encounter+0x60 -> actor+0x1ca).
 *   - Conditionally resets the actor's aggression/combat state fields
 *     (actor+0x1e4 = 0, actor+0x1e8 = -1) when encounter+0x47 == 0.
 *   - If the actor has a valid platoon index (actor+0x3c != -1), looks up the
 *     platoon definition and copies platoon+0x00 into actor+0x1c9.
 *   - If the platoon lookup result indicates a valid squad assignment
 *     (result[1] != 0 and result[2] == 0), reads the target squad index from
 *     squad_def+0x4e, bounds-checks it against the encounter's squad count,
 *     and calls FUN_0003baa0 to move the actor to that squad, then calls
 *     FUN_00036dc0 to update the actor's firing state from platoon flags.
 * After the actor loop, calls FUN_0005d890 for encounter cleanup.
 *
 * Confirmed:
 *   - cdecl, 1 stack arg (encounter_handle), RET (no stack fixup).
 *   - actor linked list: encounter+0x14 = head; actor+0x2c = next handle.
 *   - Loop guard: *(char*)(ai_globals+1) != 0 && handle != -1.
 *   - Batch ADD ESP,0x18 cleanup for FUN_0003baa0 + FUN_00036dc0 (3+3 args).
 *   - EDI = encounter record ptr, restored from [EBP-0x8] on loop-back
 * (0x5dc72).
 *   - [EBP-0x10] saves actor_handle for use as arg1 in
 * FUN_0003baa0/FUN_00036dc0.
 *
 * Call-site: FUN_0005de80 @ 0x5df5e: PUSH EDX ([EBP-0x8] = encounter_handle).
 */
void FUN_0005dc00(int encounter_handle)
{
  char *encounter;
  char *scenario;
  char *encounter_def;
  char *actor;
  char *platoon_def;
  char *squad_def;
  char *platoon_entry;
  int actor_handle;
  int saved_actor_handle;
  int next_handle;
  int squad_count;
  int16_t squad_target_idx;
  char uVar9;
  char bVar2;
  unsigned int flags_dword;

  encounter = (char *)datum_get(*(data_t **)0x5ab270, encounter_handle);

  scenario = (char *)global_scenario_get();
  encounter_def = (char *)tag_block_get_element(
    (void *)(scenario + 0x42c), encounter_handle & 0xffff, 0xb0);

  actor_handle = -1;
  if (*(char *)(*(int *)0x632574 + 1) != '\0') {
    if (encounter_handle == -1) {
      actor_handle = *(int *)(*(int *)0x632574 + 8);
    } else {
      actor_handle = *(int *)(encounter + 0x14);
    }
  }

  for (;;) {
    if (*(char *)(*(int *)0x632574 + 1) == '\0')
      break;
    if (actor_handle == -1)
      break;

    saved_actor_handle = actor_handle;
    actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);

    next_handle = *(int *)(actor + 0x2c);
    *(char *)(actor + 0x1c8) = *(char *)(encounter + 0x42);
    *(char *)(actor + 0x1ca) = *(char *)(encounter + 0x60);

    if (*(char *)(encounter + 0x47) == '\0') {
      *(int16_t *)(actor + 0x1e4) = 0;
      *(int *)(actor + 0x1e8) = -1;
    }

    uVar9 = '\0';
    bVar2 = 0;
    if (*(int16_t *)(actor + 0x3c) != -1) {
      platoon_entry =
        (char *)FUN_00054020(encounter, (int)*(int16_t *)(actor + 0x3c));
      uVar9 = platoon_entry[0];
      if (platoon_entry[1] != '\0' && platoon_entry[2] == '\0') {
        bVar2 = 1;
      }
    }
    *(char *)(actor + 0x1c9) = uVar9;

    if (bVar2) {
      squad_def = (char *)tag_block_get_element(
        (void *)(encounter_def + 0x80), (int)*(int16_t *)(actor + 0x3a), 0xe8);
      platoon_def = (char *)tag_block_get_element(
        (void *)(encounter_def + 0x8c), (int)*(int16_t *)(actor + 0x3c), 0xac);
      squad_target_idx = *(int16_t *)(squad_def + 0x4e);
      squad_count = *(int *)(encounter_def + 0x80);
      if ((int)squad_target_idx >= 0 && (int)squad_target_idx < squad_count) {
        FUN_0003baa0(saved_actor_handle, encounter_handle, squad_target_idx);
        flags_dword = *(unsigned int *)(platoon_def + 0x20);
        FUN_00036dc0(saved_actor_handle,
                     (char)((int)(flags_dword >> 1) & (int)0xffffff01u),
                     (char)(*(unsigned char *)(platoon_def + 0x20) & 1u));
      }
    }

    actor_handle = next_handle;
  }

  FUN_0005d890();
}

/* 0x5ddc0 — Iterate all encounters and reset tallies for those matching
 * the current BSP or with the "not-automatically-recycled" flag cleared.
 * Uses a data iterator over encounter_data; for each encounter whose
 * dirty flag (+0xd) is set, fetches the encounter's tag definition and
 * calls FUN_0005d910 to reset vote tallies. */
void FUN_0005ddc0(void)
{
  char *scenario;
  data_iter_t iter;
  int encounter_handle;
  char *encounter;
  char *encounter_def;
  char flag;

  scenario = (char *)global_scenario_get();
  if (*(char *)(*(int *)0x632574 + 1) != '\0') {
    data_iterator_new(&iter, *(data_t **)0x5ab270);
    flag = '\0';
  }
  for (;;) {
    if (*(char *)(*(int *)0x632574 + 1) == '\0')
      return;
    do {
      encounter = (char *)data_iterator_next(&iter);
      if (encounter == NULL || flag == '\0')
        break;
    } while (*(char *)(encounter + 0xd) == '\0');
    encounter_handle = (int)iter.datum_handle;
    if (encounter == NULL)
      return;
    encounter_def = (char *)tag_block_get_element(
      scenario + 0x42c, encounter_handle & 0xffff, 0xb0);
    if (((*(int *)0x5ac9f4 ^ encounter_handle) & 0xffff) == 0 ||
        (~*(unsigned char *)(encounter_def + 0x20) & 1) != 0) {
      FUN_0005d910(encounter_handle, -1, -1);
    }
  }
}

/* 0x5de80 — Per-tick encounter update. Every 30 ticks calls FUN_0005d890 and
 * FUN_0005a6e0. Then iterates all encounters; for each dirty encounter whose
 * handle index mod 15 matches the current tick mod 15, runs the full suite of
 * encounter update functions (tally, perception, squad management, etc.). */
void FUN_0005de80(void)
{
  int tick;
  int tick_mod15;
  data_iter_t iter;
  int encounter_handle;
  char *encounter;
  char flag;

  tick = game_time_get();
  if (tick % 30 == 0) {
    FUN_0005d890();
    FUN_0005a6e0();
  }
  tick_mod15 = tick % 15;
  if (*(char *)(*(int *)0x632574 + 1) != '\0') {
    data_iterator_new(&iter, *(data_t **)0x5ab270);
    flag = 1;
  }
  for (;;) {
    if (*(char *)(*(int *)0x632574 + 1) == '\0')
      return;
    do {
      encounter = (char *)data_iterator_next(&iter);
      if (encounter == NULL || flag == '\0')
        break;
    } while (*(char *)(encounter + 0xd) == '\0');
    encounter_handle = (int)iter.datum_handle;
    if (encounter == NULL)
      return;
    (*(short *)0x5abb34)++;
    if ((short)((encounter_handle & 0xffff) % 15) == (short)tick_mod15) {
      FUN_0005d420(encounter_handle);
      FUN_0005acf0(encounter_handle);
      FUN_0005c680(encounter_handle);
      FUN_0005ae70(encounter_handle);
      FUN_0005c940(encounter_handle);
      FUN_0005ca80(encounter_handle);
      FUN_0005dc00(encounter_handle);
    }
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
 *   FUN_0005de80  — encounter_update (needs FUN_0005acf0 @<eax> audit)
 *   FUN_0005ddc0  — encounter_tally_reset_pass (shared loop pattern)
 */
