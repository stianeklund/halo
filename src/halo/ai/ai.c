/* ai.c — AI subsystem top-level lifecycle and query functions.
 *
 * Corresponds to ai.obj (XBE address range ~0x3f5f0–0x425b0).
 * Implements initialize, dispose, dispose_from_old_map, place,
 * ai_handle_unit_approach, game_allegiance_apply_change,
 * unit_vehicle_board_notify, ai_initialize_for_new_map,
 * ai_update, ai_clump, and enemies_can_see_player entry points.
 */

/* FUN_0003f5f0: per-tick AI actor activation sweep.
 * Called from ai_update on the first-frame/map-load branch.
 * Copies ai_globals[6..7] (int16_t) into ai_globals[4..5], then clears
 * both ai_globals[6..7] and the byte flag at ai_globals[3].
 * Iterates all active player-actors (flag=1) via
 * encounter_iterator_next/FUN_00059b50. For each actor record:
 *   - if record+0xb is nonzero: calls actor_erase(actor_handle, 0)
 *     to delete/dispose the actor entry.
 *   - if record+0xb is zero and record+0x6a > 0: calls FUN_0003ec80(@esi)
 *     to activate the actor (full AI init sequence).
 * The datum handle comes from iter offset 0x14 (stored by FUN_00059b50).
 * Confirmed: void(void), called from ai_update at 0x41206 with no args.
 * Confirmed: FUN_0003ec80 takes @esi register arg (MOV ESI,[EBP-8]; CALL).
 * Confirmed: actor_erase is cdecl with 2 stack args (PUSH 0; PUSH EAX; CALL;
 * ADD ESP,8). */
void FUN_0003f5f0(void)
{
  char *g;
  char iter[0x1c]; /* extended AI actor iterator */
  char *record;

  g = *(char **)0x632574;

  /* rotate scheduling counters: copy [6..7] into [4..5], clear [6..7] and flag
   * [3] */
  *(int16_t *)(g + 4) = *(int16_t *)(g + 6);
  *(int16_t *)(g + 6) = 0;
  *(char *)(g + 3) = 0;

  /* iterate over all active player-actors */
  encounter_iterator_next(iter, 1);
  record = (char *)FUN_00059b50(iter);
  while (record != 0) {
    if (*(char *)(record + 0xb) != 0) {
      /* actor marked for deletion — dispose it */
      actor_erase(*(int *)(iter + 0x14), 0);
    } else {
      if (*(int16_t *)(record + 0x6a) > 0) {
        /* actor ready for activation — full init via @esi */
        FUN_0003ec80(*(int *)(iter + 0x14));
      }
    }
    record = (char *)FUN_00059b50(iter);
  }
}

/* ai_initialize: allocate AI globals and initialize all AI subsystems.
 * Allocates 0x8dc bytes via game_state_malloc, stores the pointer at
 * global 0x632574, zeroes the block, then calls 9 subsystem init
 * functions in order. The last call (actor_move_get_avoidance_direction) is a
 * tail-call (JMP in the original binary). Confirmed: PUSH order for
 * game_state_malloc("ai globals", NULL, 0x8dc); assert string "ai_globals" at
 * line 0x8c (140) of ai.c. */
void ai_initialize(void)
{
  void *globals = game_state_malloc("ai globals", 0, 0x8dc);
  *(void **)0x632574 = globals;
  if (!globals) {
    display_assert("ai_globals", "c:\\halo\\SOURCE\\ai\\ai.c", 0x8c, 1);
    system_exit(-1);
  }
  csmemset(*(void **)0x632574, 0, 0x8dc);
  FUN_00048e90();
  set_real_point3d();
  FUN_0005df80();
  actors_initialize();
  FUN_00064100();
  FUN_00058eb0();
  FUN_000540b0();
  FUN_00042a30();
  actor_move_get_avoidance_direction();
}

/* ai_dispose: shut down all AI subsystems in reverse-init order.
 * Calls eight subsidiary dispose functions and tail-calls the last one.
 * Confirmed: 7 CALL + 1 JMP (tail call to FUN_00048f50) in disassembly. */
void ai_dispose(void)
{
  FUN_00042b80();
  ai_profile_dispose();
  encounters_dispose();
  FUN_00064140();
  actors_dispose();
  FUN_0005df90();
  ai_debug_lineoffire_success();
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
  encounter_compute_activation_cluster_bit_vector();
  FUN_00064160();
  actors_dispose_from_old_map();
  FUN_0005dfb0();
  ai_debug_lineofsight_reset();
  FUN_00048fa0();
  /* clear the AI active flag (offset 1 in the AI globals block) */
  *(char *)(*(int *)0x632574 + 1) = 0;
}

/* ai_place: JMP thunk — forwards directly to encounters_create_for_new_map.
 * The binary at 0x3f760 is a single JMP instruction; the real body
 * lives at 0x5ddc0 (not yet identified as a named symbol). */
void ai_place(void)
{
  encounters_create_for_new_map();
}

/* 0x3f770 — Set the first byte of the AI globals block.
 * Asserts ai_globals is non-null, then writes param_1 to the first byte. */
void FUN_0003f770(char param_1)
{
  if (*(char **)0x632574 == NULL) {
    display_assert("ai_globals", "c:\\halo\\SOURCE\\ai\\ai.c", 0x13a, 1);
    system_exit(-1);
  }
  **(char **)0x632574 = param_1;
}

/* 0x3f7b0 — Set byte at ai_globals+0x10. */
void FUN_0003f7b0(char param_1)
{
  if (*(char **)0x632574 == NULL) {
    display_assert("ai_globals", "c:\\halo\\SOURCE\\ai\\ai.c", 0x143, 1);
    system_exit(-1);
  }
  *(char *)(*(char **)0x632574 + 0x10) = param_1;
}

/* 0x3f800 — Set byte at ai_globals+0x3b4. */
void FUN_0003f800(char param_1)
{
  if (*(char **)0x632574 == NULL) {
    display_assert("ai_globals", "c:\\halo\\SOURCE\\ai\\ai.c", 0x14c, 1);
    system_exit(-1);
  }
  *(char *)(*(char **)0x632574 + 0x3b4) = param_1;
}

/* 0x3f850 — Decode a force-type enum into force_major/is_random/random_chance.
 *
 * Translates an AI force-type code (param_1) into a trio of output values:
 *   force_major   — non-zero when the force is a "major" (definitive) event
 *   is_random     — non-zero when the force has a random-chance component
 *   random_chance — the probability [0,1] fetched from game globals difficulty
 *
 * Force-type mapping (from assert string: "force_major && is_random && random_chance"):
 *   1 (default): is_random=1, random_chance=FUN_000b5590(0x1c)  (normal random)
 *   2:           is_random=1, random_chance=FUN_000b5590(0x1d)  (variant 1)
 *   3:           is_random=1, random_chance=FUN_000b5590(0x1e)  (variant 2)
 *   4:           is_random=0, force_major=0  (non-random, non-major)
 *   5:           is_random=0, force_major=1  (non-random, major)
 *
 * Confirmed: 4 cdecl stack args; no register args; no return value.
 * Confirmed: ADD ESP,0x14 after display_assert+system_exit pair at 0x3f888.
 * Confirmed: MOVSX EAX, word [EBP+8]; DEC EAX; CMP EAX,3; JA default (jump table 1-4).
 * Confirmed: ESI=[EBP+0x10]=is_random, EDI=[EBP+0xc]=force_major, EBX=[EBP+0x14]=random_chance.
 * Confirmed: case 1 at 0x3f8b2: MOV [ESI],1; PUSH 0x1d; CALL FUN_000b5590; FSTP [EBX].
 * Confirmed: case 2 at 0x3f8c6: MOV [ESI],1; PUSH 0x1e; CALL; FSTP [EBX].
 * Confirmed: case 3 at 0x3f89c: MOV [ESI],0; MOV [EDI],0.
 * Confirmed: case 4 at 0x3f8a7: MOV [ESI],0; MOV [EDI],1.
 * Confirmed: default at 0x3f8da: MOV [ESI],1; PUSH 0x1c; CALL; FSTP [EBX].
 */
void FUN_0003f850(int16_t param_1, char *force_major, char *is_random, float *random_chance)
{
  if ((force_major == NULL) || (is_random == NULL) || (random_chance == NULL)) {
    display_assert("force_major && is_random && random_chance",
                   "c:\\halo\\SOURCE\\ai\\ai.c", 0x158, 1);
    system_exit(-1);
  }
  switch (param_1) {
  case 1:
    *is_random = 1;
    *random_chance = FUN_000b5590(0x1d);
    return;
  case 2:
    *is_random = 1;
    *random_chance = FUN_000b5590(0x1e);
    return;
  case 3:
    *is_random = 0;
    *force_major = 0;
    return;
  case 4:
    *is_random = 0;
    *force_major = 1;
    return;
  default:
    *is_random = 1;
    *random_chance = FUN_000b5590(0x1c);
    return;
  }
}

/* FUN_0003f970: erase AI actors matching an encounter/squad/squad-group filter.
 * Guards on AI globals active flag (*(char*)(ai_globals+1) != 0).
 * If param_1 == -1 (all encounters): iterates all actors via
 *   encounter_iterator_next (flag=0) + FUN_00059b50; erases each via
 *   actor_erase(iter+0x14 handle, param_4).
 * Else: initialises a per-encounter actor iterator via
 *   encounter_actor_iterator_new(&iter, param_1) + FUN_00059a50; for each
 *   actor, skips if actor+0x3c != param_2 (unless param_2==-1) or
 *   actor+0x3a != param_3 (unless param_3==-1); erases matching actors via
 *   actor_erase(iter[1] handle, param_4).
 *
 * Stack layout (SUB ESP,0x28):
 *   [EBP-0x28..EBP-0x15]: enc_iter[0x1c] (encounter iterator, all-branch)
 *   [EBP-0x14]:           enc_iter+0x14 (actor handle field in iterator)
 *   [EBP-0x0c..EBP-0x09]: actor_iter[2] (encounter-actor iterator, single-branch)
 *   [EBP-0x08]:           actor_iter[1] (actor handle, 4 bytes into actor_iter)
 *
 * Confirmed: PUSH 0x0 at 0x3f992 → encounter_iterator_next flag=0.
 * Confirmed: MOVSX+CMP for short fields at actor+0x3c (param_2) and
 *            actor+0x3a (param_3).
 * Confirmed: actor_erase args: PUSH param_4, PUSH actor_handle (cdecl). */
void FUN_0003f970(int param_1, int param_2, int param_3, int param_4)
{
  char enc_iter[0x1c]; /* encounter iterator for the all-encounters branch */
  int actor_iter[2];   /* encounter-actor iterator: [0]=state, [1]=handle */
  int has_more;
  int actor;

  if (*(char *)(*(int *)0x632574 + 1) == 0) {
    return;
  }

  if (param_1 == -1) {
    /* iterate all actors across all encounters */
    encounter_iterator_next(enc_iter, 0);
    has_more = FUN_00059b50(enc_iter);
    while (has_more != 0) {
      actor_erase(*(int *)(enc_iter + 0x14), (char)param_4);
      has_more = FUN_00059b50(enc_iter);
    }
  } else {
    /* iterate actors within the specified encounter, applying filters */
    encounter_actor_iterator_new(actor_iter, param_1);
    actor = FUN_00059a50(actor_iter);
    while (actor != 0) {
      if ((param_2 == -1 || *(short *)(actor + 0x3c) == param_2) &&
          (param_3 == -1 || *(short *)(actor + 0x3a) == param_3)) {
        actor_erase(actor_iter[1], (char)param_4);
      }
      actor = FUN_00059a50(actor_iter);
    }
  }
}

/* ai_handle_unit_approach: test whether a unit is approaching a valid
 * target for an AI actor, and optionally record the approach.
 * Looks up the actor via actor_data, checks the unit against
 * object_get_and_verify_type (type 3 = unit), tests team friendship via
 * game_allegiance_get_team_is_friendly, and returns true (1) when the
 * teams are NOT friendly (i.e. the unit is an enemy worth approaching).
 * If flag is non-zero and the check passes, sets the approach-active
 * flag at actor+0x2ed by calling FUN_00036e30.
 * Confirmed: 3 args (PUSH count), no ADD ESP after final CALL, bool
 * return via AL; ADD ESP,8 after each of the two inner calls. */
bool ai_handle_unit_approach(int ai_handle, int unit_handle, bool flag)
{
  char *actor;
  char *unit;
  bool result;

  actor = datum_get(actor_data, ai_handle);
  result = 0;
  if (unit_handle != -1) {
    unit = object_get_and_verify_type(unit_handle, 3);
    if (*(int *)(unit + 0x1c8) != -1) {
      /* game_allegiance_get_team_is_friendly returns true when friendly;
       * we return true (enemy) only when NOT friendly. */
      if (!game_allegiance_get_team_is_friendly(*(int16_t *)(unit + 0x68),
                                                *(int16_t *)(actor + 0x3e))) {
        result = 1;
        if (flag) {
          /* set the approach-active flag at actor+0x2ed */
          FUN_00036e30(ai_handle);
        }
      }
    }
  }
  return result;
}

/* game_allegiance_apply_change: apply an allegiance change between two
 * teams, updating all matching actor records in the AI actor iterator.
 * Iterates over all active player-actors via
 * encounter_iterator_next/FUN_00059b50; for each actor whose team matches
 * team_a or team_b, walks the actor's clump items via FUN_00064540/FUN_00064570
 * and applies the friendship and force flags. Confirmed: 4 args via PUSH count
 * + ADD ESP,0x18 cleanup at 0x40068. Operand sizes confirmed: team_a/team_b as
 * int16_t (MOVSX + CMP AX,DI); friendship/force as char (MOV byte ptr).
 *
 * Stack layout (EBP-based, SUB ESP,0x24):
 *   [EBP-0x24..EBP-0x09]: ai_actor_iter (0x1c bytes extended iterator)
 *   [EBP-0x08]:           clump_item_iter[0] (current clump-item handle)
 *   [EBP-0x04]:           clump_item_iter[1] (next clump-item handle)
 * Note: [EBP-0x10] == ai_actor_iter.field_0x14 (current actor handle);
 *       the decompiler names it 'local_14' because it overlaps the iter. */
void game_allegiance_apply_change(int16_t team_a, int16_t team_b,
                                  char friendship, char force)
{
  char iter[0x1c]; /* extended AI actor iterator; see encounter_iterator_next */
  int clump_iter[2]; /* clump-item walk: [0]=current handle, [1]=next */
  int16_t matched_team;
  int actor;
  int clump_item;

  /* optional debug console print */
  if (*(char *)0x5aca55) {
    const char *perm = force ? " permanently" : "";
    const char *rel = friendship ? "broken" : "reformed";
    console_printf(0, "allegiance between teams %s and %s %s%s",
                   ((const char **)0x2efdf8)[team_a],
                   ((const char **)0x2efdf8)[team_b], rel, perm);
  }

  /* initialise iterator over all active player-actors (flag=1) */
  encounter_iterator_next(iter, 1);
  actor = FUN_00059b50(iter);
  while (actor != 0) {
    /* check if this actor belongs to team_a or team_b */
    matched_team = team_b;
    if (*(int16_t *)(actor + 0x3e) == team_a) {
      matched_team = team_b;
    } else if (*(int16_t *)(actor + 0x3e) == team_b) {
      matched_team = team_a;
    } else {
      goto next_actor;
    }
    if (matched_team == -1) {
      goto next_actor;
    }

    /* walk this actor's clump items, using actor handle from iter.field_0x14 */
    FUN_00064540(clump_iter, *(int *)(iter + 0x14));
    clump_item = FUN_00064570(clump_iter);
    while (clump_item != 0) {
      if (*(int16_t *)(clump_item + 0x12) == matched_team) {
        if (!force) {
          /* mark clump item fields +0x61 and +0x62 */
          *(char *)(clump_item + 0x61) = 1;
          *(char *)(clump_item + 0x62) = 1;
        }
        if (!friendship || force) {
          *(char *)(clump_item + 0x60) = friendship;
          *(char *)(clump_item + 0xa4) = actor_get_perception_knowledge(
            *(int *)(iter + 0x14), clump_iter[0]);
          *(float *)(clump_item + 0x50) =
            FUN_0002fd10(*(int *)(iter + 0x14), clump_iter[0]);
        }
      }
      clump_item = FUN_00064570(clump_iter);
    }

  next_actor:
    actor = FUN_00059b50(iter);
  }
}

/* 0x40280 — Update clump-item perception fields for all active AI actors.
 * Iterates all active actors; for each actor's clump items, retrieves the
 * unit's team, checks friendliness/hostility against the actor's team,
 * and computes perception visibility and distance. */
void FUN_00040280(void)
{
  char iter[0x1c];
  int clump_iter[2];
  int actor;
  int clump_item;
  int unit;
  short team;

  encounter_iterator_next(iter, 1);
  actor = FUN_00059b50(iter);
  while (actor != 0) {
    FUN_00064540(clump_iter, *(int *)(iter + 0x14));
    clump_item = FUN_00064570(clump_iter);
    while (clump_item != 0) {
      unit = (int)object_get_and_verify_type(*(int *)(clump_item + 0x18), 3);
      team = *(short *)(unit + 0x68);
      *(short *)(clump_item + 0x12) = team;
      *(char *)(clump_item + 0x60) = game_allegiance_get_team_is_friendly(
        *(short *)(actor + 0x3e), (int)team);
      *(char *)(clump_item + 0x61) =
        FUN_000a7a90(*(short *)(actor + 0x3e), *(short *)(clump_item + 0x12));
      *(char *)(clump_item + 0xa4) =
        actor_get_perception_knowledge(*(int *)(iter + 0x14), clump_iter[0]);
      *(float *)(clump_item + 0x50) =
        FUN_0002fd10(*(int *)(iter + 0x14), clump_iter[0]);
      clump_item = FUN_00064570(clump_iter);
    }
    actor = FUN_00059b50(iter);
  }
}

/* FUN_00040570: spawn AI actors into vehicle seats from pending vehicle list.
 * Called each tick from ai_update. Iterates the vehicle spawn queue stored
 * in the AI globals block: a count at offset +0x8b8 (int16_t) and an array
 * of object handles starting at offset +0x8bc. For each queued vehicle,
 * looks up its unit tag definition and walks the tag_block at tag+0x2e4
 * (element size 0x11c). For each seat element with a valid actor variant
 * tag index at element+0x104, creates an actor via FUN_0003f030 using the
 * vehicle's world position as the starting location, then boards the new
 * actor's unit into the vehicle at the corresponding seat index.
 * Clears the queue count to zero after processing.
 *
 * Confirmed: void(void) — no args, no return value.
 * Confirmed: outer loop uses CMP SI (16-bit comparison).
 * Confirmed: inner loop counter sign-extended via MOVSX EAX,AX.
 * Confirmed: csmemset size 0x1c, word at buffer+0x1a = 0xffff.
 * Confirmed: FUN_0003f030 args: 6 pushes, ADD ESP,0x2c (cleans 6 args + prior
 * 5). Confirmed: unit_board_vehicle args: PUSH EDX(seat), PUSH EDI(vehicle),
 * PUSH EAX(unit). */
void FUN_00040570(void)
{
  int g;
  int vehicle_handle;
  char *unit_obj;
  char *tag_data;
  int *seats_block;
  char *seat_element;
  int actor_handle;
  char *actor;
  char starting_location[0x1c];
  int16_t i;
  int16_t j;

  g = *(volatile int *)0x632574;
  if (*(int16_t *)(g + 0x8b8) < 1) {
    *(int16_t *)(g + 0x8b8) = 0;
    return;
  }

  for (i = 0; i < *(int16_t *)(g + 0x8b8); i++) {
    g = *(volatile int *)0x632574;
    vehicle_handle = *(int *)(g + 0x8bc + (int16_t)i * 4);
    unit_obj = object_get_and_verify_type(vehicle_handle, 3);
    tag_data = tag_get(0x756e6974, *(int *)unit_obj);
    seats_block = (int *)(tag_data + 0x2e4);

    for (j = 0; (int)j < *seats_block; j++) {
      seat_element = tag_block_get_element(seats_block, (int)j, 0x11c);
      if (*(int *)(seat_element + 0x104) != -1) {
        csmemset(starting_location, 0, 0x1c);
        *(int16_t *)(starting_location + 0x1a) = (int16_t)0xffff;
        object_get_world_position(vehicle_handle,
                                  (vector3_t *)starting_location);
        actor_handle = FUN_0003f030(*(int *)(seat_element + 0x104), -1, -1,
                                    starting_location, 0, 0);
        if (actor_handle != -1) {
          actor = datum_get(actor_data, actor_handle);
          unit_board_vehicle(*(int *)(actor + 0x18), vehicle_handle, j);
        }
      }
    }
  }

  *(int16_t *)(*(volatile int *)0x632574 + 0x8b8) = 0;
}

/* unit_vehicle_board_notify: notify the AI subsystem that a unit is boarding
 * a vehicle. Verifies the unit object (type_mask=3 for biped|vehicle), and
 * if the unit has an AI actor (offset 0x1a4 != -1), dispatches an AI
 * command via FUN_00046f10 with command type 0x24.
 * The vehicle_handle parameter is accepted but unused in this function body.
 * Confirmed: 1 stack param used ([EBP+8]), second param ([EBP+0xc]) untouched.
 * Confirmed: PUSH order for FUN_00046f10 — 7 args, cdecl (ADD ESP,0x1c). */
void unit_vehicle_board_notify(int unit_handle, int vehicle_handle)
{
  void *unit_obj = object_get_and_verify_type(unit_handle, 3);
  if (*(int *)((char *)unit_obj + 0x1a4) != -1) {
    FUN_00046f10(0x24, unit_handle, -1, -1, -1, -1, 0);
  }
}

/* ai_initialize_for_new_map: reset the AI globals block and initialise
 * all per-map AI subsystems.
 * Zeroes the 0x8dc-byte globals block (at *(int*)0x632574), writes
 * initial state into known fields, calls 8 per-map init helpers, then
 * zeroes the scheduling counters and finally sets the AI active flag
 * (globals[1]) to mark the subsystem ready.
 *
 * Store-offset table for writes to *(int*)0x632574 (derived from disasm):
 *   +0x00 (byte): 1   — first-frame flag
 *   +0x02 (byte): 1   — second flag
 *   +0x08 (dword): -1 — initial actor handle sentinel
 *   +0x10 (byte): 1   — flag
 *   +0x14..+0x1b: 0xff fill (8 bytes via csmemset)
 *   +0x1c..+0x23: 0xff fill (8 bytes via csmemset)
 *   +0x24..+0x2b: 0xff fill (8 bytes via csmemset)
 *   +0x130 (word): 0  — schedule count
 *   +0x132 (word): 0  — schedule index
 *   +0x134..+0x3b3: 0 fill (0x280 bytes via csmemset)
 *   +0x3b4 (byte): 1  — another flag
 *   +0x01 (byte): 1   — AI active flag (set last)
 *
 * Confirmed: ADD ESP,0x3c at 0x41172 cleans all 5 csmemset call arg
 * triples (5×3=15 args, 15×4=60=0x3c). */
void ai_initialize_for_new_map(void)
{
  int *g = *(int **)0x632574;

  csmemset(g, 0, 0x8dc);
  *(char *)((char *)g + 0x00) = 1;
  *(char *)((char *)g + 0x02) = 1;
  *(int *)((char *)g + 0x08) = -1;
  *(char *)((char *)g + 0x3b4) = 1;
  *(char *)((char *)g + 0x10) = 1;
  csmemset((char *)g + 0x14, -1, 8);
  csmemset((char *)g + 0x1c, -1, 8);
  csmemset((char *)g + 0x24, -1, 8);

  FUN_0004c0f0();
  FUN_00053650();
  FUN_0005dfa0();
  actor_in_combat();
  FUN_00064150();
  FUN_0005b200();
  FUN_000540d0();
  FUN_00042b90();

  *(int16_t *)((char *)g + 0x132) = 0;
  *(int16_t *)((char *)g + 0x130) = 0;
  csmemset((char *)g + 0x134, 0, 0x280);

  /* mark AI subsystem active */
  *(char *)((char *)g + 0x01) = 1;
}

/* ai_update: per-tick AI update dispatcher.
 * Reads the AI active flag from globals[1] and the pause flag from
 * 0x5abaa0 to decide whether to run the main update. If the scheduling
 * flag 0x5abaa1 (cVar2) is set, runs actor_update_scripted branch;
 * otherwise branches on globals[0] to run normal actor updates or
 * process accumulated spawns (globals[2]).
 * Wraps the update body in profile_enter_private / profile_exit_private
 * when profile_global_enable (0x449ef1) and the profile enable flag
 * (0x2c8738) are both set.
 * Confirmed: ai_active = globals[1], globals[0] = first-frame flag,
 * globals[2] = pending-spawn flag; all byte accesses verified in disasm. */
void ai_update(void)
{
  bool should_update;
  char schedule_flag;

  schedule_flag = *(char *)0x5abaa1;

  /* check AI active and not paused */
  if (*(char *)(*(int *)0x632574 + 1) && !*(char *)0x5abaa0) {
    should_update = 1;
  } else {
    should_update = 0;
  }

  if (*(bool *)0x449ef1 && *(char *)0x2c8738) {
    profile_enter_private(*(void **)0x2c8730);
  }

  if (should_update) {
    FUN_0004ab10();
    FUN_00053680();
    FUN_00040570();
    if (schedule_flag) {
      /* scripted/scheduled actor branch */
      actors_move_randomly();
      *(char *)(*(int *)0x632574 + 2) = 1;
    } else {
      if (*(char *)*(int *)0x632574) {
        /* first-frame / map-load branch */
        ai_conversation_update();
        FUN_0005de80();
        FUN_0003f5f0();
        *(char *)(*(int *)0x632574 + 2) = 1;
      } else {
        /* accumulated-spawn branch */
        if (*(char *)(*(int *)0x632574 + 2)) {
          actors_freeze();
          *(char *)(*(int *)0x632574 + 2) = 0;
        }
      }
    }
  }

  if (*(bool *)0x449ef1 && *(char *)0x2c8738) {
    profile_exit_private(*(void **)0x2c8730);
  }
}

/* FUN_000413c0: fill one ai_firing_pos_entry_t in the candidate buffer.
 *
 * Register args (thunk loads before CALL):
 *   ESI = ai_firing_pos_entry_t *entry  — pointer to the slot to fill
 *   EDI = int unit_handle               — the object handle passed through
 *                                         as entry->handle_b and as arg to
 *                                         biped_get_camera_height_and_offset
 *
 * Stack arg:
 *   [EBP+0x8] = int actor_handle        — stored as entry->handle_a
 *
 * Calls biped_get_camera_height_and_offset(unit_handle, &entry->vec_a[0],
 *   &height_offset, &camera_height) to populate the biped's eye position.
 *
 * is_sphere:  height_offset == 0.0f (height_offset at [EBP-4];
 *             compared against DAT_002533c0 = 0.0f via FCOMP).
 * scalar_a:   height_offset ([EBP-4]).
 * radius:     camera_height ([EBP-8]) + DAT_00256140 (~0.15f).
 * handle_a:   actor_handle ([EBP+0x8]).
 * handle_b:   unit_handle (EDI).
 * occupied:   0 (CL = 0, XOR ECX,ECX done before test; written last).
 * vec_b[0/1]: 0 (ECX = 0).
 *
 * Store-offset table (from disasm, NOT decompiler):
 *   [ESI+0x01] = is_sphere            MOV byte [ESI+1], AL (0x41402)
 *   [ESI+0x10] = vec_b[0] = 0         MOV dword [ESI+0x10], ECX (0x41408)
 *   [ESI+0x14] = vec_b[1] = 0         MOV dword [ESI+0x14], ECX (0x4140b)
 *   [ESI+0x18] = scalar_a             MOV dword [ESI+0x18], EAX (0x4140e)
 *   [ESI+0x24] = radius (fstp)        FSTP float [ESI+0x24]     (0x41411)
 *   [ESI+0x1c] = handle_a             MOV dword [ESI+0x1c], EDX (0x41414)
 *   [ESI+0x20] = handle_b (EDI)       MOV dword [ESI+0x20], EDI (0x41417)
 *   [ESI+0x00] = occupied = 0         MOV byte [ESI], CL        (0x4141a)
 *
 * Note: MSVC reorders stores (pipeline scheduling). occupied written last
 * even though it logically comes first. Preserved here in disasm order.
 *
 * Confirmed: cdecl, 1 stack arg, RET (no stack cleanup in callee).
 * Confirmed: ADD ESP,0x10 at 0x413e1 cleans all 4 pushes to 0x1a0890. */
void FUN_000413c0(ai_firing_pos_entry_t *entry, int unit_handle,
                  int actor_handle)
{
  float height_offset;
  float camera_height;

  biped_get_camera_height_and_offset(unit_handle, (vector3_t *)entry->vec_a,
                                     &height_offset, &camera_height);

  /* is_sphere: true when biped has no height offset (eye at ground level) */
  entry->is_sphere = (height_offset == 0.0f);
  entry->vec_b[0] = 0.0f;
  entry->vec_b[1] = 0.0f;
  entry->scalar_a = height_offset;
  entry->radius = camera_height + *(float *)0x256140;
  entry->handle_a = actor_handle;
  entry->handle_b = unit_handle;
  entry->occupied = 0;
}

/* FUN_00041420: build the firing-position candidate list for an actor.
 *
 * Iterates two linked lists:
 *   1. The actor's own encounter clump (via
 * encounter_actor_iterator_new/FUN_00059a50 on actor->clump_handle at
 * actor_record+0x34).  For each member:
 *        - skip if member handle == actor_handle (self)
 *        - skip if count >= max_count
 *        - skip if member has no object (member+0x18 == -1)
 *        - skip if member is already targeting something (member+0x158 != -1)
 *        Calls FUN_00064ab0(actor_handle, member_object_handle) to get a
 *        staging handle, then FUN_000413c0(@esi=entry, @edi=object_handle,
 *        actor_handle_from_64ab0) to fill the slot.
 *
 *   2. A secondary prop/enemy list (via FUN_00064540/FUN_00064570).
 *      For each entry:
 *        - skip if entry+0x60 is nonzero (flag)
 *        - skip if entry+0x127 is nonzero (flag)
 *        - skip if weapon-slot type != 3 (entry+0x24)
 *        - skip if entry+0x110 != -1
 *        - verify via object_get_and_verify_type that object type bit 0 is set
 *        - skip if both are in the same encounter (same encounter handle)
 *        - skip if count >= max_count
 *        Calls FUN_000413c0(@esi=entry, @edi=entry_object_handle,
 *        local_10[0]) to fill the slot.
 *
 * Returns count of candidates written (int16_t in BX, returned via AX).
 *
 * Entry pointer arithmetic (confirmed from disasm):
 *   MOVSX EAX,BX               ; EAX = count (sign-extended)
 *   LEA EAX,[EAX + EAX*4]      ; EAX = count * 5
 *   LEA ESI,[buf + EAX*8]      ; ESI = buf + count * 0x28
 *
 * Confirmed: 3 stack args, cdecl, returns int16_t in AX (MOV AX,BX at epilog).
 * Confirmed: ESI restored to param_1 at 0x4149f/0x414a2 after inner call.
 * Confirmed: BX used as count throughout; EBX callee-saved across all calls.
 * Confirmed: first loop iterator at [EBP-0x10] (12 bytes: handle/current/next).
 *            second loop iterator at [EBP-0xc] (8 bytes: actor_handle/next).
 *
 * Call-site verification table — call to FUN_000413c0 at 0x4149a:
 *   arg      | binary source         | C expr             | match?
 *   stack[0] | PUSH EAX (ret 64ab0) | actor_handle_64ab0 | YES
 *   @esi     | LEA ESI,[buf+cnt*40] | &buf[count]        | YES
 *   @edi     | MOV EDI,[EDI+0x18]   | member_object_hdl  | YES
 *
 * Call-site verification table — call to FUN_000413c0 at 0x41567:
 *   arg      | binary source         | C expr             | match?
 *   stack[0] | PUSH ECX ([EBP-0xc]) | local_10[0]        | YES
 *   @esi     | LEA ESI,[buf+cnt*40] | &buf[count]        | YES
 *   @edi     | MOV EDI,[EDI+0x18]   | prop_object_handle | YES */
int16_t FUN_00041420(int actor_handle, int16_t max_count,
                     ai_firing_pos_entry_t *buf)
{
  char *actor;
  char *member;
  char *prop;
  char *prop_obj;
  int prop_obj_handle;
  int staging;
  int member_object_handle;
  int iter_a[3]; /* [EBP-0x10]: encounter-clump iterator (12 bytes) */
  int local_10[2]; /* [EBP-0xc]: prop-list iterator (8 bytes) */
  int16_t count;

  actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
  count = 0;

  /* --- loop 1: encounter clump members --- */
  if (*(int *)(actor + 0x34) != -1) {
    encounter_actor_iterator_new(iter_a, *(int *)(actor + 0x34));
    member = (char *)FUN_00059a50(iter_a);
    while (member) {
      if (iter_a[1] != actor_handle && count < max_count &&
          *(int *)(member + 0x18) != -1 && *(int *)(member + 0x158) == -1) {
        member_object_handle = *(int *)(member + 0x18);
        staging = FUN_00064ab0(actor_handle, member_object_handle);
        /* entry ptr = buf + count*0x28; EDI = member_object_handle */
        FUN_000413c0(&buf[count], member_object_handle, staging);
        count++;
      }
      member = (char *)FUN_00059a50(iter_a);
    }
  }

  /* --- loop 2: prop / enemy list --- */
  FUN_00064540(local_10, actor_handle);
  prop = (char *)FUN_00064570(local_10);
  while (prop) {
    if (*(char *)(prop + 0x60) == 0 && *(char *)(prop + 0x127) == 0 &&
        *(int16_t *)(prop + 0x24) == 3 && *(int *)(prop + 0x110) == -1) {
      prop_obj_handle = *(int *)(prop + 0x18);
      prop_obj = (char *)object_get_and_verify_type(prop_obj_handle, (int)-1);
      if ((1 << (*(unsigned char *)(prop_obj + 0x64) & 0x1f) & 1u) != 0) {
        /* same-encounter filter */
        if (*(int *)(actor + 0x34) == -1 || *(int *)(prop + 0x1c) == -1 ||
            *(int *)((char *)datum_get(*(void **)0x6325a4,
                                       *(int *)(prop + 0x1c)) +
                     0x34) != *(int *)(actor + 0x34)) {
          if (count < max_count) {
            prop_obj_handle = *(int *)(prop + 0x18);
            /* entry ptr = buf + count*0x28; EDI = prop_obj_handle */
            FUN_000413c0(&buf[count], prop_obj_handle, local_10[0]);
            count++;
          }
        }
      }
    }
    prop = (char *)FUN_00064570(local_10);
  }

  return count;
}

/* ai_firing_pos_entry_t: see types.h for layout. */

/* ai_test_line_of_fire: test whether the actor can fire at a target through any
 * candidate firing position, and return the best candidate handle.
 *
 * Builds up to 0x20 candidate firing-position entries via FUN_00041420
 * (collecting nearby cover points / target-prop positions), then for each
 * entry:
 *   - skips entries whose handle_b matches excluded_handle (param_2)
 *   - if entry.is_sphere: calls FUN_0010bc70 (line-sphere intersection test)
 *   - otherwise:          calls FUN_0010e040 (segment-segment proximity test)
 * On the first passing test, marks that entry as occupied, stores its
 * handle_a as the result datum, clears the success flag, and breaks.
 *
 * When ai_debug lineoffire rendering is active (0x5aca69 != 0), records
 * the session begin/end and logs each entry via ai_debug helpers.
 *
 * Returns: 1 (bool true) if a valid position was found, 0 otherwise.
 * Output:  *result_out = handle_a of winning entry (-1 if none).
 *
 * Confirmed: 5 args, cdecl, ADD ESP,0x14 at call site (0x00023e02).
 * Confirmed: return in AL (low byte of success flag; 1=found, 0=not found).
 * Confirmed: global INC at 0x5ac6e4 = entry-attempt counter (word).
 * Confirmed: guard 0x5aca69 = ai_debug lineoffire enable flag.
 * Confirmed: EBX = param_5 (int *result_out) loaded at 0x000415cc AFTER
 *   the FUN_00041420 call+cleanup. EBX is callee-saved and used throughout.
 * Confirmed: buf size = 0x508 bytes (SUB ESP,0x508; buf at EBP-0x508). */
bool ai_test_line_of_fire(int actor_handle, int excluded_handle, float *origin,
                          float *offset, int *result_out)
{
  ai_firing_pos_entry_t buf[0x20]; /* 0x20 entries × 0x28 = 0x500 bytes */
  int result_datum;
  bool success;
  int i, count;

  datum_get(*(void **)0x6325a4, actor_handle);
  *(int16_t *)0x5ac6e4 += 1;

  success = 1;
  result_datum = -1;

  count = (int)(int16_t)FUN_00041420(actor_handle, 0x20, buf);

  if (count > 0) {
    for (i = 0; i < count; i++) {
      ai_firing_pos_entry_t *e = &buf[i];

      /* skip entries whose exclusion handle matches param_2 */
      if (e->handle_b == excluded_handle) {
        continue;
      }

      {
        bool hit;
        if (e->is_sphere) {
          /* push-then-fstp pattern: radius is loaded via FLD then
           * FSTP [ESP] after PUSH ECX (dummy). Confirmed at 0x41600:
           * FLD [EBP+EAX+0xfffffb1c]; PUSH ECX; FSTP [ESP]. */
          hit = FUN_0010bc70(origin, offset, e->vec_a, e->radius);
        } else {
          hit = FUN_0010e040(origin, offset, e->vec_a, e->vec_b, e->scalar_a);
        }

        if (hit) {
          result_datum = e->handle_a;
          success = 0;
          e->occupied = 1;
          break;
        }
      }
    }
  }

  /* ai_debug lineoffire rendering */
  if (*(char *)0x5aca69) {
    ai_debug_get_last_path(origin, offset);
    for (i = 0; i < count; i++) {
      ai_firing_pos_entry_t *e = &buf[i];
      FUN_00049430(e->vec_a, e->vec_b, *(int *)&e->radius, e->occupied);
    }
    FUN_000494d0((char)success);
  }

  if (result_out) {
    *result_out = result_datum;
  }
  return (bool)success;
}

/* ai_clump (FUN_00042390): scan all active player-actor records to find
 * any actor that should trigger a clump (grouping) response.
 * Iterates via data_iterator_new/data_iterator_next over the data at
 * 0x5ab23c. For each record: checks active/valid flags, verifies the
 * unit is a vehicle occupant, looks up the actor, selects target via
 * actor->field_6, retrieves the unit tag and checks the 0x80000 flag.
 * Various clump-eligibility conditions are tested (swarm flag, state
 * range, timers, squads) with float comparisons from constants embedded
 * in the binary. Returns 1 (true) if any clump-eligible actor is found,
 * 0 otherwise.
 * Confirmed: param_1 is char (PUSH 0 / PUSH 1 at call sites);
 * return via AL = 0 or 1 (two separate RETs); two exit paths.
 * Inferred: 0x5ab23c = swarm/clump data_t; 0x2533d8, 0x254cc0,
 * 0x254cc8, 0x254e74 are float constants embedded in the game binary. */
bool FUN_00042390(char param_1)
{
  int current_time;
  data_iter_t iter; /* standard 0x10-byte data iterator */
  char *rec;
  char *unit;
  char *actor;
  char *tag;
  bool bVar4;
  int16_t state;

  current_time = game_time_get();
  data_iterator_new(&iter, *(data_t **)0x5ab23c);
  rec = data_iterator_next(&iter);

  do {
    if (rec == 0) {
      return 0;
    }

    /* check active (0x12e) and enabled (0x60) flags */
    if (*(char *)(rec + 0x12e) && *(char *)(rec + 0x60)) {
      char *unit_rec;
      /* verify unit is a vehicle occupant (type 3), with a rider (1c8) */
      unit = object_get_and_verify_type(*(int *)(rec + 0x18), 3);
      if (*(int *)(unit + 0x1c8) != -1) {
        /* look up actor for this record */
        actor = datum_get(actor_data, *(int *)(rec + 0x4));

        /* select target handle based on actor->field_6 */
        if (*(char *)(actor + 0x6)) {
          unit_rec = (char *)*(int *)(actor + 0x24);
        } else {
          unit_rec = (char *)*(int *)(actor + 0x18);
        }

        unit_rec = object_get_and_verify_type((int)unit_rec, 3);
        tag = tag_get(0x756e6974, *(int *)unit_rec);

        /* check unit tag has swarm flag (bit 19 = 0x80000) */
        bVar4 = 0;
        if (*(int *)(tag + 0x17c) & 0x80000) {
          /* compare distance to constant at 0x2533d8 */
          bVar4 = *(float *)(rec + 0x11c) > *(float *)0x2533d8;
        }

        /* if param_1 set and actor is not already clumped/in-range,
         * apply proximity override check against constant at 0x254cc0 */
        if (param_1 && *(int16_t *)(actor + 0x5f2) == 0 &&
            *(int16_t *)(actor + 0x6c) != 10) {
          if (*(float *)(rec + 0x11c) <= *(float *)0x254cc0) {
            goto next_rec;
          }
        }

        if (bVar4) {
          goto next_rec;
        }

        state = *(int16_t *)(rec + 0x24);

        /* states outside [4,5]: check timer handle */
        if ((state < 4 || state > 5) && *(int *)(rec + 0x8c) != -1 &&
            *(int *)(rec + 0x8c) + 0x5a >= current_time) {
          return 1;
        }

        /* states outside [4,5]: compare distance again */
        if ((state < 4 || state > 5)) {
          if (*(float *)(rec + 0x11c) < *(float *)0x2533d8) {
            return 1;
          }
        }

        /* squad check: actor->field_0x270 must match iter.datum_handle
         * (iter.datum_handle = EBP-0xc in disassembly, overlaps the
         * decompiler's 'local_10' variable) */
        if (*(int *)(actor + 0x270) == (int)iter.datum_handle) {
          if (state > 1 && state <= 3) {
            return 1;
          }
          if (state >= 4 && state <= 5) {
            /* check leading-actor flag and squad membership */
            if (*(char *)(rec + 0xb8)) {
              return 1;
            }
            /* look up leader actor via record->field_0xc */
            actor = datum_get(*(data_t **)0x5ab23c, *(int *)(rec + 0xc));
            if (*(int16_t *)(rec + 0x24) == 4 &&
                *(float *)(rec + 0x11c) < *(float *)0x254cc8) {
              /* distance-squared check between two position fields */
              if (distance_squared3d((void *)(actor + 0xbc),
                                     (void *)(rec + 0xbc)) <
                  *(float *)0x254e74) {
                return 1;
              }
            }
          }
        }
      }
    }

  next_rec:
    rec = data_iterator_next(&iter);
  } while (1);
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

/* ai_enemies_attacking_player: unconditionally trigger a clump check with
 * flag=1. Thin wrapper around ai_clump (FUN_00042390). Return value is
 * discarded by the caller. Confirmed: PUSH 1 / CALL 0x42390 / ADD ESP,4 / RET.
 */
void ai_enemies_attacking_player(void)
{
  FUN_00042390(1);
}

/* FUN_000425c0: ai_sound_spatial_effect_submit — submit a spatial sound effect
 * into the AI's ring buffer of recent spatial events. If AI subsystem is
 * inactive (g+1 == 0), returns immediately.
 *
 * The ring buffer lives at g+0x130 (head/tail uint16_t pair) and holds up to
 * 32 entries of 0x14 bytes each starting at g+0x134.  Each entry:
 *   +0x00  int16_t  effect_type
 *   +0x02  int16_t  count          (number of samples blended in)
 *   +0x04  float[3] position       (running-average world position)
 *   +0x10  int      timestamp      (game_time when last sample was stored)
 *
 * Algorithm: walk the ring from head to tail.  For each live entry:
 *   - If its timestamp is older than (now - 120): expire it (mark 0xffff,
 *     advance head if it was the head slot, or record as a free slot).
 *   - Else if effect_type matches and distance from stored position to the
 *     new position is less than 1.0 (distance_squared3d < 1.0): merge
 *     the new sample in.  If timestamp is stale by >= 30 ticks, overwrite
 *     the position directly (first new sample); otherwise blend it in as
 *     a running average (weight = 1/count for new, 1 - 1/count for old).
 *     Break out of the loop after merging.
 * If no matching entry was found, allocate either the first expired slot or
 * the current tail (advancing tail and bumping head if the queue is full),
 * then write the entry and call FUN_0003c0c0 to trigger the actual AI reaction.
 *
 * Asserts: count > 0, 0 <= volume < 5, 0 <= effect_type < 3.
 * Confirmed: c:\halo\SOURCE\ai\ai.c line 0x80e/0x80f/0x810/0x847/0x871.
 */
void FUN_000425c0(int object_handle, float *position, short effect_type,
                  short volume, short count)
{
  int *g;
  int current_time;
  int time_minus_30;
  int time_minus_120;
  char submit;
  short *entry;
  short i;
  int free_slot;
  short dist_to_end;
  short queue_len;
  float inv_count;
  float old_weight;

  g = *(int **)0x632574;
  if (*((char *)g + 1) == '\0') {
    return;
  }
  current_time = game_time_get();

  if (count < 1) {
    display_assert("count>0", "c:\\halo\\SOURCE\\ai\\ai.c", 0x80e, 1);
    system_exit(-1);
  }
  if ((volume < 0) || (volume >= 5)) {
    display_assert("volume>=0 && volume<NUMBER_OF_AI_SOUND_VOLUMES",
                   "c:\\halo\\SOURCE\\ai\\ai.c", 0x80f, 1);
    system_exit(-1);
  }
  if ((effect_type < 0) || (effect_type >= 3)) {
    display_assert("effect_type>=0 && effect_type<NUMBER_OF_AI_SPATIAL_EFFECTS",
                   "c:\\halo\\SOURCE\\ai\\ai.c", 0x810, 1);
    system_exit(-1);
  }

  if (volume <= 0) {
    return;
  }

  time_minus_30 = current_time - 0x1e;
  time_minus_120 = current_time - 0x78;

  entry = (short *)0x0;
  submit = 1;
  free_slot = -1;

  for (i = *(short *)((char *)g + 0x130); i != *(short *)((char *)g + 0x132);
       i = (short)((i + 1) & 0x1f)) {
    char *slot;
    char match;

    match = 0;
    slot = (char *)g + 0x134 + (int)i * 0x14;

    if (effect_type == *(short *)slot &&
        distance_squared3d((float *)(slot + 0x4), position) <
          *(float *)0x2533c8) {
      match = 1;
    }

    if (*(int *)(slot + 0x10) > time_minus_120) {
      if (match) {
        entry = (short *)slot;
        entry[1] = entry[1] + 1;
        submit = (*(int *)(slot + 0x10) < time_minus_30);
        if (*(int *)(slot + 0x10) < time_minus_30) {
          *(float *)(slot + 0x4) = position[0];
          *(float *)(slot + 0x8) = position[1];
          *(float *)(slot + 0xc) = position[2];
          if (!submit) {
            display_assert("submit", "c:\\halo\\SOURCE\\ai\\ai.c", 0x847, 1);
            system_exit(-1);
          }
        } else {
          inv_count = *(float *)0x2533c8 / (float)(int)entry[1];
          old_weight = *(float *)0x2533c8 - inv_count;
          *(float *)(slot + 0x4) =
            inv_count * position[0] + old_weight * *(float *)(slot + 0x4);
          *(float *)(slot + 0x8) =
            inv_count * position[1] + old_weight * *(float *)(slot + 0x8);
          *(float *)(slot + 0xc) =
            inv_count * position[2] + old_weight * *(float *)(slot + 0xc);
        }
        break;
      }
    } else {
      *(short *)slot = (short)0xffff;
      if (i == *(short *)((char *)g + 0x130)) {
        *(short *)((char *)g + 0x130) = (short)((i + 1) & 0x1f);
      } else {
        free_slot = (int)i;
      }
    }
  }

  if (entry == (short *)0x0) {
    if (free_slot == -1) {
      i = *(short *)((char *)g + 0x132);
      *(short *)((char *)g + 0x132) =
        (short)((*(short *)((char *)g + 0x132) + 1) & 0x1f);
      if (*(short *)((char *)g + 0x132) == *(short *)((char *)g + 0x130)) {
        *(short *)((char *)g + 0x130) =
          (short)((*(short *)((char *)g + 0x130) + 1) & 0x1f);
      }
    } else {
      i = (short)free_slot;
    }

    dist_to_end = (short)((i - *(short *)((char *)g + 0x130) + 0x20) & 0x1f);
    queue_len = (short)((*(short *)((char *)g + 0x132) -
                         *(short *)((char *)g + 0x130) + 0x20) &
                        0x1f);

    if (!((dist_to_end >= 0) && (dist_to_end < queue_len))) {
      display_assert("(distance_to_effect >= 0) && (distance_to_effect < "
                     "distance_to_end_of_queue)",
                     "c:\\halo\\SOURCE\\ai\\ai.c", 0x871, 1);
      system_exit(-1);
    }

    entry = (short *)((char *)g + 0x134 + (int)i * 0x14);
    *(float *)((char *)entry + 0x4) = position[0];
    *(float *)((char *)entry + 0x8) = position[1];
    *(float *)((char *)entry + 0xc) = position[2];
    *(int *)((char *)entry + 0x10) = current_time;
    *entry = effect_type;
    entry[1] = 1;
  }

  if (submit) {
    FUN_0003c0c0(object_handle, *entry, (float *)((char *)entry + 0x4), volume,
                 entry[1]);
  }
}

/* FUN_0003fa40: count and erase swarm units, format a result description.
 *
 * Iterates all AI actors via encounter_iterator_next (flag=0) + FUN_00059b50.
 * For each actor record where:
 *   record[6] != 0  (actor is active/alive)
 *   record[8] == 0  (not in some suppressed state)
 *   *(int*)(record+0xc) != -1  (has a valid reference)
 * accumulates *(short*)(record+0x1e) into swarm_count, then erases the actor
 * via actor_erase(handle, 1).
 *
 * After iteration: formats "%d swarm units" into result_description via
 * crt_sprintf, sets *more_to_release = 0, returns 1 if swarm_count > 0.
 *
 * Stack layout (SUB ESP,0x20):
 *   [EBP-0x20..EBP-0xd]: iter[0x14] (encounter iterator, 20-byte body)
 *   [EBP-0xc]:           iter+0x14  (actor handle stored by FUN_00059b50)
 *   [EBP-0x4]:           local_8    (initialized to 0; base for swarm_count/SI)
 *
 * Confirmed: assert string "result_description && more_to_release", line 0x1f7=503.
 * Confirmed: encounter_iterator_next flag=0 (PUSH 0x0 at 0x3fa81).
 * Confirmed: MOV EDX,[EBP-0xc]; PUSH EDX as actor_erase first arg (handle at iter+0x14).
 * Confirmed: ADD SI,word[EAX+0x1e] accumulates short field at record+0x1e.
 * Confirmed: MOVSX ECX,SI; PUSH ECX; PUSH fmt; PUSH EDI → crt_sprintf(result_desc,...).
 * Confirmed: XOR EAX,EAX; TEST SI,SI; SETG AL → returns 1 if swarm_count > 0.
 * Confirmed: MOV byte[EBX],0x0 → *more_to_release = 0. */
int FUN_0003fa40(int result_description, char *more_to_release)
{
  char iter[24]; /* encounter iterator; actor handle at iter+0x14 */
  int record;
  short swarm_count;

  swarm_count = 0;

  if ((result_description == 0) || (more_to_release == (char *)0x0)) {
    display_assert("result_description && more_to_release",
                   "c:\\halo\\SOURCE\\ai\\ai.c", 0x1f7, 1);
    system_exit(-1);
  }

  encounter_iterator_next(iter, 0);
  record = FUN_00059b50(iter);
  while (record != 0) {
    if ((*(char *)(record + 6) != '\0') &&
        (*(char *)(record + 8) == '\0') &&
        (*(int *)(record + 0xc) != -1)) {
      swarm_count = (short)(swarm_count + *(short *)(record + 0x1e));
      actor_erase(*(int *)(iter + 0x14), 1);
    }
    record = FUN_00059b50(iter);
  }

  crt_sprintf((char *)result_description, "%d swarm units", (int)swarm_count);
  *more_to_release = 0;
  return swarm_count > 0;
}

/* Compare two AI records for sorting. Primary key: int at offset 8 (ascending).
   Secondary key: unsigned byte at offset 0 (ascending).
   Returns 1 if param_1 < param_2, -1 if param_1 > param_2, 0 if equal. */
int FUN_0003fb00(unsigned char *param_1, unsigned char *param_2)
{
  if (*(int *)(param_2 + 8) < *(int *)(param_1 + 8)) {
    return 1;
  }
  if (*(int *)(param_1 + 8) < *(int *)(param_2 + 8)) {
    return 0xffffffff;
  }
  if (*param_2 < *param_1) {
    return 0xffffffff;
  }
  return (*param_1 < *param_2);
}

/* FUN_0003fe30: resolve a vehicle unit handle to an occupant handle.
 *
 * Given a vehicle unit handle, returns the handle of a key occupant:
 *  - If prefer_passenger is nonzero and the vehicle has a passenger
 *    (unit+0x2d8 != -1), resolve to the passenger handle.
 *  - Otherwise, if the vehicle has a driver (unit+0x2d4 != -1), resolve
 *    to the driver handle.
 *  - If no resolution changed the handle (remains -1 or unchanged), returns
 *    the resolved handle directly.
 *
 * Debug filter (applied only in non-multiplayer with debug flag set):
 *  - If game_connection() != 0 (multiplayer): skip filter, return resolved.
 *  - If byte[0x5ac9c6] == 0 (AI debug flag off): skip filter, return resolved.
 *  - If the resolved unit has no rider (unit+0x1c8 == -1): skip filter,
 *    return resolved.
 *  - Otherwise (debug on, singleplayer, resolved unit has a rider): return -1.
 *
 * Confirmed: param_1=[EBP+8] (int), prefer_passenger=[EBP+C] (char).
 * Returns int in EAX. */
int FUN_0003fe30(int unit_handle, char prefer_passenger)
{
  void *obj;
  int resolved;
  int candidate;

  if (unit_handle == -1) {
    return -1;
  }
  obj = object_try_and_get_and_verify_type(unit_handle, 3);
  if (obj == NULL) {
    return -1;
  }

  resolved = unit_handle;
  if (prefer_passenger != '\0') {
    candidate = *(int *)((char *)obj + 0x2d8);
    if (candidate != -1) {
      resolved = candidate;
      goto check_debug;
    }
  }
  candidate = *(int *)((char *)obj + 0x2d4);
  if (candidate != -1) {
    resolved = candidate;
  }

check_debug:
  if (resolved == -1) {
    return resolved;
  }
  if (game_connection() != 0) {
    return resolved;
  }
  if (*(char *)0x5ac9c6 == '\0') {
    return resolved;
  }
  obj = object_get_and_verify_type(resolved, 3);
  if (*(int *)((char *)obj + 0x1c8) == -1) {
    return resolved;
  }
  return -1;
}

/* FUN_0003feb0: Notify AI systems when a unit exits a vehicle.
 *
 * param_1 (unit_handle): the unit that just exited.
 * param_2: passed to FUN_0003fe30 as unit_handle for vehicle-occupant resolution.
 * param_3: vehicle/context handle; used to control prefer_passenger (word !=9)
 *          and forwarded as param5 of FUN_00046f10.
 *
 * Resolves the occupant from param_2 via FUN_0003fe30, determines a relationship
 * code (0=same unit, 2=enemy, 3=friendly, or 0xffffffff if no valid occupant),
 * then notifies the AI communication system (FUN_00046f10), clears encounter
 * references (FUN_00044660), and updates encounter kill counts (FUN_0005b2a0).
 *
 * Confirmed: [EBP+8]=unit_handle (int), [EBP+C]=param_2 (int),
 *            [EBP+10]=param_3 compared as word ptr. */
void FUN_0003feb0(int unit_handle, int param_2, short param_3)
{
  char relation;
  int resolved;
  void *obj_unit;
  void *obj_resolved;

  resolved = FUN_0003fe30(param_2, (char)(param_3 != 9));
  relation = '\0';
  if (unit_handle == resolved) {
    relation = '\x01';
  } else if (resolved != -1) {
    obj_unit     = object_get_and_verify_type(unit_handle, 3);
    obj_resolved = object_get_and_verify_type(resolved, 3);
    relation = (char)(game_allegiance_get_team_is_friendly(
                  *(short *)((char *)obj_resolved + 0x68),
                  *(short *)((char *)obj_unit     + 0x68)) != '\0') + '\x02';
  }
  FUN_00046f10(0, unit_handle, resolved, (int)relation, (int)param_3, -1, 0);
  FUN_00044660(unit_handle, '\0');
  FUN_0005b2a0(unit_handle);
}
