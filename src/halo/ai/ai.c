/* ai.c — AI subsystem top-level lifecycle and query functions.
 *
 * Corresponds to ai.obj (XBE address range ~0x3f5f0–0x425c0).
 * Implements initialize, dispose, dispose_from_old_map, place,
 * ai_handle_unit_approach, game_allegiance_apply_change,
 * unit_vehicle_board_notify, ai_initialize_for_new_map,
 * ai_update, ai_clump, enemies_can_see_player, and
 * ai_spatial_effect_submit entry points.
 */

/* FUN_0003f5f0: per-tick AI actor activation sweep.
 * Called from ai_update on the first-frame/map-load branch.
 * Copies ai_globals[6..7] (int16_t) into ai_globals[4..5], then clears
 * both ai_globals[6..7] and the byte flag at ai_globals[3].
 * Iterates all active player-actors (flag=1) via FUN_00059b10/FUN_00059b50.
 * For each actor record:
 *   - if record+0xb is nonzero: calls FUN_0003d950(actor_handle, 0)
 *     to delete/dispose the actor entry.
 *   - if record+0xb is zero and record+0x6a > 0: calls FUN_0003ec80(@esi)
 *     to activate the actor (full AI init sequence).
 * The datum handle comes from iter offset 0x14 (stored by FUN_00059b50).
 * Confirmed: void(void), called from ai_update at 0x41206 with no args.
 * Confirmed: FUN_0003ec80 takes @esi register arg (MOV ESI,[EBP-8]; CALL).
 * Confirmed: FUN_0003d950 is cdecl with 2 stack args (PUSH 0; PUSH EAX; CALL;
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
  FUN_00059b10(iter, 1);
  record = (char *)FUN_00059b50(iter);
  while (record != 0) {
    if (*(char *)(record + 0xb) != 0) {
      /* actor marked for deletion — dispose it */
      FUN_0003d950(*(int *)(iter + 0x14), 0);
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
 * functions in order. The last call (FUN_0002b5d0) is a tail-call
 * (JMP in the original binary).
 * Confirmed: PUSH order for game_state_malloc("ai globals", NULL, 0x8dc);
 * assert string "ai_globals" at line 0x8c (140) of ai.c. */
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
  FUN_00053620();
  FUN_0005df80();
  FUN_0003a990();
  FUN_00064100();
  FUN_00058eb0();
  FUN_000540b0();
  FUN_00042a30();
  FUN_0002b5d0();
}

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
 * Iterates over all active player-actors via FUN_00059b10/FUN_00059b50;
 * for each actor whose team matches team_a or team_b, walks the actor's
 * clump items via FUN_00064540/FUN_00064570 and applies the friendship
 * and force flags.
 * Confirmed: 4 args via PUSH count + ADD ESP,0x18 cleanup at 0x40068.
 * Operand sizes confirmed: team_a/team_b as int16_t (MOVSX + CMP AX,DI);
 * friendship/force as char (MOV byte ptr).
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
  char iter[0x1c]; /* extended AI actor iterator; see FUN_00059b10 */
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
  FUN_00059b10(iter, 1);
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
          *(char *)(clump_item + 0xa4) =
            FUN_0002fc20(*(int *)(iter + 0x14), clump_iter[0]);
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

  FUN_00059b10(iter, 1);
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
        FUN_0002fc20(*(int *)(iter + 0x14), clump_iter[0]);
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
  FUN_0003aa60();
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
      FUN_0003ba00();
      *(char *)(*(int *)0x632574 + 2) = 1;
    } else {
      if (*(char *)*(int *)0x632574) {
        /* first-frame / map-load branch */
        FUN_00046cb0();
        FUN_0005de80();
        FUN_0003f5f0();
        *(char *)(*(int *)0x632574 + 2) = 1;
      } else {
        /* accumulated-spawn branch */
        if (*(char *)(*(int *)0x632574 + 2)) {
          FUN_0003b900();
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
 *   1. The actor's own encounter clump (via FUN_00059a00/FUN_00059a50 on
 *      actor->clump_handle at actor_record+0x34).  For each member:
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
    FUN_00059a00(iter_a, *(int *)(actor + 0x34));
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

/* FUN_00041590: test whether the actor can fire at a target through any
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
bool FUN_00041590(int actor_handle, int excluded_handle, float *origin,
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
    FUN_000493d0(origin, offset);
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

/* FUN_000425b0: unconditionally trigger a clump check with flag=1.
 * Thin wrapper around ai_clump (FUN_00042390). Return value is discarded
 * by the caller.
 * Confirmed: PUSH 1 / CALL 0x42390 / ADD ESP,4 / RET. */
void FUN_000425b0(void)
{
  FUN_00042390(1);
}

/* FUN_000425c0: submit a spatial AI effect to the ring buffer and dispatch
 * it to nearby actors.
 *
 * Maintains a circular queue of 32 entries at ai_globals+0x134. Each entry
 * is 0x14 bytes: { short type, short count, float position[3], int timestamp }.
 * Head/tail indices at ai_globals+0x130 / 0x132, wrapping at 32 (mask 0x1f).
 *
 * When a new effect arrives:
 *   1. Scan existing queue entries for a match (same effect_type, position
 *      within 1.0 world unit). Expire entries older than 120 ticks.
 *   2. If a match is found and its timestamp is recent (within 30 ticks),
 *      blend the position; otherwise overwrite it. Increment count.
 *   3. If no match: allocate a new slot (reuse a freed one, or advance tail).
 *   4. If the submit flag is set: call FUN_0003c0c0 to apply the spatial
 *      effect to all actors within range.
 *
 * Confirmed: cdecl, 5 stack args, returns void.
 * Confirmed: callers at 0xf8d18 and 0xfa63a push 5 args, ADD ESP,0x14. */
void FUN_000425c0(int object_handle, float *position, short effect_type,
                  short volume, short count)
{
  char *g;
  int game_time;
  int threshold_submit;
  int threshold_position;
  int threshold_expiry;
  short *effect;
  int submit;
  short free_slot;
  short idx;
  int nearby;
  short dist_to_effect;
  short dist_to_end;
  float weight;
  float one_minus_weight;
  int count_int;

  if (*(char *)(*(char **)0x632574 + 1) == '\0')
    return;

  game_time = game_time_get();

  if (count < 1) {
    display_assert("count>0", "c:\\halo\\SOURCE\\ai\\ai.c", 0x80e, 1);
    system_exit(-1);
  }
  if (volume < 0 || 4 < volume) {
    display_assert("volume>=0 && volume<NUMBER_OF_AI_SOUND_VOLUMES",
                   "c:\\halo\\SOURCE\\ai\\ai.c", 0x80f, 1);
    system_exit(-1);
  }
  if (effect_type < 0 || 2 < effect_type) {
    display_assert(
        "effect_type>=0 && effect_type<NUMBER_OF_AI_SPATIAL_EFFECTS",
        "c:\\halo\\SOURCE\\ai\\ai.c", 0x810, 1);
    system_exit(-1);
  }

  if (0 < volume) {
    int ofs;
    effect = (short *)0;
    submit = 1;
    free_slot = -1;

    threshold_submit = game_time - 0x1e;
    threshold_position = game_time - 0x1e;
    threshold_expiry = threshold_position - 0x5a;

    /* scan the ring buffer for a match or expired entries */
    g = *(char **)0x632574;
    for (idx = *(short *)(g + 0x130);
         idx != *(short *)(*(char **)0x632574 + 0x132);
         idx = (short)((idx + 1) & 0x1f)) {

      nearby = 0;
      ofs = idx * 0x14;

      /* check for matching type and nearby position */
      if (effect_type == *(short *)(*(char **)0x632574 + 0x134 + ofs) &&
          distance_squared3d(
              (const float *)(*(char **)0x632574 + 0x138 + ofs),
              position) < *(float *)0x2533c8) {
        nearby = 1;
      }

      /* check if entry has expired */
      if (*(int *)(*(char **)0x632574 + 0x144 + ofs) > threshold_expiry)
      {
        if (nearby) {
          /* found a matching entry — update it */
          effect = (short *)(*(char **)0x632574 + 0x134 + ofs);
          effect[1] = (short)(effect[1] + 1);

          submit = *(int *)(effect + 8) < threshold_submit;

          if (*(int *)(effect + 8) < threshold_position) {
            /* entry is old enough — overwrite position directly */
            *(float *)(effect + 2) = position[0];
            *(float *)(effect + 4) = position[1];
            *(float *)(effect + 6) = position[2];
            if (!submit) {
              display_assert("submit", "c:\\halo\\SOURCE\\ai\\ai.c", 0x847, 1);
              system_exit(-1);
            }
          } else {
            /* blend position with existing entry */
            count_int = (int)effect[1];
            weight = *(float *)0x2533c8 / (float)count_int;
            one_minus_weight = *(float *)0x2533c8 - weight;
            *(float *)(effect + 2) =
                weight * position[0] +
                one_minus_weight * *(float *)(effect + 2);
            *(float *)(effect + 4) =
                weight * position[1] +
                one_minus_weight * *(float *)(effect + 4);
            *(float *)(effect + 6) =
                weight * position[2] +
                one_minus_weight * *(float *)(effect + 6);
          }
          break;
        }
      } else {
        /* entry expired — mark as unused */
        *(short *)(*(char **)0x632574 + 0x134 + ofs) = -1;
        if (idx == *(short *)(*(char **)0x632574 + 0x130)) {
          *(short *)(*(char **)0x632574 + 0x130) = (short)((idx + 1) & 0x1f);
        } else {
          free_slot = idx;
        }
      }
    }

    /* if no match was found, allocate a new slot */
    if (effect == (short *)0) {
      if (free_slot == -1) {
        /* no free slot — advance tail, possibly wrap head */
        g = *(char **)0x632574;
        idx = *(short *)(g + 0x132);
        *(short *)(*(char **)0x632574 + 0x132) =
            (short)((*(short *)(*(char **)0x632574 + 0x132) + 1) & 0x1f);
        if (*(short *)(*(char **)0x632574 + 0x132) ==
            *(short *)(*(char **)0x632574 + 0x130)) {
          *(short *)(*(char **)0x632574 + 0x130) =
              (short)((*(short *)(*(char **)0x632574 + 0x130) + 1) & 0x1f);
        }
      } else {
        idx = free_slot;
      }

      /* assert the new slot is within the valid range */
      dist_to_effect =
          (short)(((idx - *(short *)(*(char **)0x632574 + 0x130)) + 0x20) &
                  0x1f);
      dist_to_end =
          (short)(((*(short *)(*(char **)0x632574 + 0x132) -
                    *(short *)(*(char **)0x632574 + 0x130)) +
                   0x20) &
                  0x1f);
      if (dist_to_effect < 0 || dist_to_effect >= dist_to_end) {
        display_assert(
            "(distance_to_effect >= 0) && (distance_to_effect < "
            "distance_to_end_of_queue)",
            "c:\\halo\\SOURCE\\ai\\ai.c", 0x871, 1);
        system_exit(-1);
      }

      /* initialize the new entry */
      effect = (short *)(*(char **)0x632574 + 0x134 + idx * 0x14);
      *(float *)(effect + 2) = position[0];
      *(float *)(effect + 4) = position[1];
      *(float *)(effect + 6) = position[2];
      *(int *)(effect + 8) = game_time;
      *effect = effect_type;
      effect[1] = 1;
    }

    /* dispatch the spatial effect to actors if submit flag is set */
    if (submit) {
      FUN_0003c0c0(object_handle, *effect, (float *)(effect + 2), volume,
                   effect[1]);
    }
  }
}
