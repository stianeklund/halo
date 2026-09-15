/* ai.c — AI subsystem top-level lifecycle and query functions.
 *
 * Corresponds to ai.obj (XBE address range ~0x3f5f0–0x425b0).
 * Implements initialize, dispose, dispose_from_old_map, place,
 * ai_handle_unit_approach, game_allegiance_apply_change,
 * unit_vehicle_board_notify, ai_initialize_for_new_map,
 * ai_update, ai_clump, and enemies_can_see_player entry points.
 */

/* actors_update: per-tick AI actor activation sweep.
 * Called from ai_update on the first-frame/map-load branch.
 * Copies ai_globals[6..7] (int16_t) into ai_globals[4..5], then clears
 * both ai_globals[6..7] and the byte flag at ai_globals[3].
 * Iterates all active player-actors (flag=1) via
 * actor_iterator_new/actor_iterator_next. For each actor record:
 *   - if record+0xb is nonzero: calls actor_erase(actor_handle, 0)
 *     to delete/dispose the actor entry.
 *   - if record+0xb is zero and record+0x6a > 0: calls actor_update(@esi)
 *     to activate the actor (full AI init sequence).
 * The datum handle comes from iter offset 0x14 (stored by actor_iterator_next).
 * Confirmed: void(void), called from ai_update at 0x41206 with no args.
 * Confirmed: actor_update takes @esi register arg (MOV ESI,[EBP-8]; CALL).
 * Confirmed: actor_erase is cdecl with 2 stack args (PUSH 0; PUSH EAX; CALL;
 * ADD ESP,8). */
void actors_update(void)
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
  actor_iterator_new(iter, 1);
  record = (char *)actor_iterator_next(iter);
  while (record != 0) {
    if (*(char *)(record + 0xb) != 0) {
      /* actor marked for deletion — dispose it */
      actor_erase(*(int *)(iter + 0x14), 0);
    } else {
      if (*(int16_t *)(record + 0x6a) > 0) {
        /* actor ready for activation — full init via @esi */
        actor_update(*(int *)(iter + 0x14));
      }
    }
    record = (char *)actor_iterator_next(iter);
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
  ai_debug_initialize();
  ai_profile_initialize();
  paths_initialize();
  actors_initialize();
  props_initialize();
  encounters_initialize();
  ai_script_initialize();
  ai_communication_initialize();
  actor_move_get_avoidance_direction();
}

/* ai_dispose: shut down all AI subsystems in reverse-init order.
 * Calls eight subsidiary dispose functions and tail-calls the last one.
 * Confirmed: 7 CALL + 1 JMP (tail call to ai_debug_dispose) in disassembly. */
void ai_dispose(void)
{
  ai_communication_dispose();
  ai_script_dispose();
  encounters_dispose();
  props_dispose();
  actors_dispose();
  paths_dispose();
  ai_profile_dispose();
  ai_debug_dispose();
}

/* ai_dispose_from_old_map: release per-map AI state when leaving a map.
 * Calls eight subsidiary dispose_from_old_map helpers, then clears the
 * AI active flag (byte at offset 1 in the AI globals struct) to mark the
 * subsystem inactive. Confirmed: 8 CALLs + MOV EAX,[0x632574] / MOV
 * byte ptr [EAX+1],0 in disassembly. */
void ai_dispose_from_old_map(void)
{
  ai_communication_dispose_from_old_map();
  ai_script_dispose_from_old_map();
  encounters_dispose_from_old_map();
  props_dispose_from_old_map();
  actors_dispose_from_old_map();
  paths_dispose_from_old_map();
  ai_profile_dispose_from_old_map();
  ai_debug_dispose_from_old_map();
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
void ai_globals_ai_active(char param_1)
{
  if (*(char **)0x632574 == NULL) {
    display_assert("ai_globals", "c:\\halo\\SOURCE\\ai\\ai.c", 0x13a, 1);
    system_exit(-1);
  }
  **(char **)0x632574 = param_1;
}

/* 0x3f7b0 — Set byte at ai_globals+0x10. */
void ai_globals_dialogue_triggers_enabled(char param_1)
{
  if (*(char **)0x632574 == NULL) {
    display_assert("ai_globals", "c:\\halo\\SOURCE\\ai\\ai.c", 0x143, 1);
    system_exit(-1);
  }
  *(char *)(*(char **)0x632574 + 0x10) = param_1;
}

/* 0x3f800 — Set byte at ai_globals+0x3b4. */
void ai_globals_grenades_enabled(char param_1)
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
 * Force-type mapping (from assert string: "force_major && is_random &&
 * random_chance"): 1 (default): is_random=1, random_chance=FUN_000b5590(0x1c)
 * (normal random) 2:           is_random=1, random_chance=FUN_000b5590(0x1d)
 * (variant 1) 3:           is_random=1, random_chance=FUN_000b5590(0x1e)
 * (variant 2) 4:           is_random=0, force_major=0  (non-random, non-major)
 *   5:           is_random=0, force_major=1  (non-random, major)
 *
 * Confirmed: 4 cdecl stack args; no register args; no return value.
 * Confirmed: ADD ESP,0x14 after display_assert+system_exit pair at 0x3f888.
 * Confirmed: MOVSX EAX, word [EBP+8]; DEC EAX; CMP EAX,3; JA default (jump
 * table 1-4). Confirmed: ESI=[EBP+0x10]=is_random, EDI=[EBP+0xc]=force_major,
 * EBX=[EBP+0x14]=random_chance. Confirmed: case 1 at 0x3f8b2: MOV [ESI],1; PUSH
 * 0x1d; CALL FUN_000b5590; FSTP [EBX]. Confirmed: case 2 at 0x3f8c6: MOV
 * [ESI],1; PUSH 0x1e; CALL; FSTP [EBX]. Confirmed: case 3 at 0x3f89c: MOV
 * [ESI],0; MOV [EDI],0. Confirmed: case 4 at 0x3f8a7: MOV [ESI],0; MOV [EDI],1.
 * Confirmed: default at 0x3f8da: MOV [ESI],1; PUSH 0x1c; CALL; FSTP [EBX].
 */
void ai_get_major_upgrade_chance(int16_t param_1, char *force_major,
                                 char *is_random, float *random_chance)
{
  if ((force_major == NULL) || (is_random == NULL) || (random_chance == NULL)) {
    display_assert("force_major && is_random && random_chance",
                   "c:\\halo\\SOURCE\\ai\\ai.c", 0x158, 1);
    system_exit(-1);
  }
  switch (param_1) {
  case 3:
    *is_random = 0;
    *force_major = 0;
    return;
  case 4:
    *is_random = 0;
    *force_major = 1;
    return;
  case 1:
    *is_random = 1;
    *random_chance = FUN_000b5590(0x1d);
    return;
  case 2:
    *is_random = 1;
    *random_chance = FUN_000b5590(0x1e);
    return;
  default:
    *is_random = 1;
    *random_chance = FUN_000b5590(0x1c);
    return;
  }
}

/* 0x3f900 — ai_adjust_damage: scale incoming damage by per-actor AI modifiers.
 *
 * If actor_handle is a valid datum handle into the actor pool ([0x6325a4]):
 *   - when damage_params+0x4 has bit 3 (0x8) set AND actor->field_69c > 0.0f,
 *     multiplies *scale by actor->field_69c (an AI-specific damage multiplier);
 *   - when actor->field_1ca is non-zero, multiplies *scale by the global float
 *     at 0x2533e4 and returns 1 unconditionally.
 * Returns whether any scaling was applied.
 *
 * Confirmed: PUSH EAX(actor_handle); PUSH [0x6325a4]; CALL datum_get; ADD ESP,8
 *   at 0x3f915 — 2 cdecl args, pool is actor_data (same global as actors.c).
 * Confirmed: MOV AL,byte[ECX+0x4]; TEST AL,0x8 at 0x3f91f/0x3f928 —
 * damage_params flag byte at +0x4, bit 0x8. Confirmed: FLD [EDX+0x69c]; FCOMP
 * [0x2533c0]; FNSTSW; TEST AH,0x41; JNZ skip at 0x3f92c — proceeds only when
 * field_69c > *(float*)0x2533c0 (0.0f). Confirmed: FLD [EDX+0x69c]; FMUL [ECX];
 * FSTP [ECX] at 0x3f93f — multiplicand order is field_69c * (*scale).
 * Confirmed: MOV BL,0x1 at 0x3f945 sits between the FLD and the FMUL — the flag
 *   is assigned before the multiply.
 * Confirmed: FLD [ECX]; FMUL [0x2533e4]; FSTP [ECX] at 0x3f955 — (*scale) *
 * const. Confirmed: return value is byte-sized in AL — MOV AL,0x1 at 0x3f957 on
 * the field_1ca path, MOV AL,BL at 0x3f964 on every other exit (BL zeroed by
 *   XOR BL,BL at 0x3f907). */
bool ai_adjust_damage(int actor_handle, void *damage_params, float *scale)
{
  actor_t *actor;
  bool adjusted;

  adjusted = 0;
  if (actor_handle != -1) {
    actor = (actor_t *)datum_get(*(void **)0x6325a4, actor_handle);
    if (((*((unsigned char *)damage_params + 4) & 8) != 0) &&
        (actor->field_69c > *(float *)0x2533c0)) {
      adjusted = 1;
      *scale = actor->field_69c * *scale;
    }
    if (actor->field_1ca != '\0') {
      *scale = *scale * *(float *)0x2533e4;
      return 1;
    }
  }
  return adjusted;
}

/* ai_erase: erase AI actors matching an encounter/squad/squad-group filter.
 * Guards on AI globals active flag (*(char*)(ai_globals+1) != 0).
 * If param_1 == -1 (all encounters): iterates all actors via
 *   actor_iterator_new (flag=0) + actor_iterator_next; erases each via
 *   actor_erase(iter+0x14 handle, param_4).
 * Else: initialises a per-encounter actor iterator via
 *   encounter_actor_iterator_new(&iter, param_1) +
 * encounter_actor_iterator_next; for each actor, skips if actor+0x3c != param_2
 * (unless param_2==-1) or actor+0x3a != param_3 (unless param_3==-1); erases
 * matching actors via actor_erase(iter[1] handle, param_4).
 *
 * Stack layout (SUB ESP,0x28):
 *   [EBP-0x28..EBP-0x15]: enc_iter[0x1c] (encounter iterator, all-branch)
 *   [EBP-0x14]:           enc_iter+0x14 (actor handle field in iterator)
 *   [EBP-0x0c..EBP-0x09]: actor_iter[2] (encounter-actor iterator,
 * single-branch) [EBP-0x08]:           actor_iter[1] (actor handle, 4 bytes
 * into actor_iter)
 *
 * Confirmed: PUSH 0x0 at 0x3f992 → actor_iterator_new flag=0.
 * Confirmed: MOVSX+CMP for short fields at actor+0x3c (param_2) and
 *            actor+0x3a (param_3).
 * Confirmed: actor_erase args: PUSH param_4, PUSH actor_handle (cdecl). */
void ai_erase(int param_1, int param_2, int param_3, int param_4)
{
  char enc_iter[0x1c]; /* encounter iterator for the all-encounters branch */
  int actor_iter[2]; /* encounter-actor iterator: [0]=state, [1]=handle */
  int has_more;
  int actor;

  if (*(char *)(*(int *)0x632574 + 1) == 0) {
    return;
  }

  if (param_1 == -1) {
    /* iterate all actors across all encounters */
    actor_iterator_new(enc_iter, 0);
    has_more = actor_iterator_next(enc_iter);
    while (has_more != 0) {
      actor_erase(*(int *)(enc_iter + 0x14), (char)param_4);
      has_more = actor_iterator_next(enc_iter);
    }
  } else {
    /* iterate actors within the specified encounter, applying filters */
    encounter_actor_iterator_new(actor_iter, param_1);
    actor = encounter_actor_iterator_next(actor_iter);
    while (actor != 0) {
      if ((param_2 == -1 || ((actor_t *)actor)->field_03c == param_2) &&
          (param_3 == -1 || ((actor_t *)actor)->field_03a == param_3)) {
        actor_erase(actor_iter[1], (char)param_4);
      }
      actor = encounter_actor_iterator_next(actor_iter);
    }
  }
}

/* ai_release_inactive_swarms: count and erase swarm units, format a result
 * description.
 *
 * Iterates all AI actors via actor_iterator_new (flag=0) +
 * actor_iterator_next. For each actor record where: record[6] != 0  (actor is
 * active/alive) record[8] == 0  (not in some suppressed state)
 *   *(int*)(record+0xc) != -1  (has a valid reference)
 * accumulates *(short*)(record+0x1e) into swarm_count, then erases the actor
 * via actor_erase(handle, 1).
 *
 * After iteration: formats "%d swarm units" into result_description via
 * crt_sprintf, sets *more_to_release = 0, returns 1 if swarm_count > 0.
 *
 * Stack layout (SUB ESP,0x20):
 *   [EBP-0x20..EBP-0xd]: iter[0x14] (encounter iterator, 20-byte body)
 *   [EBP-0xc]:           iter+0x14  (actor handle stored by
 * actor_iterator_next) [EBP-0x4]:           local_8    (initialized to 0; base
 * for swarm_count/SI)
 *
 * Confirmed: assert string "result_description && more_to_release", line
 * 0x1f7=503. Confirmed: actor_iterator_new flag=0 (PUSH 0x0 at 0x3fa81).
 * Confirmed: MOV EDX,[EBP-0xc]; PUSH EDX as actor_erase first arg (handle at
 * iter+0x14). Confirmed: ADD SI,word[EAX+0x1e] accumulates short field at
 * record+0x1e. Confirmed: MOVSX ECX,SI; PUSH ECX; PUSH fmt; PUSH EDI →
 * crt_sprintf(result_desc,...). Confirmed: XOR EAX,EAX; TEST SI,SI; SETG AL →
 * returns 1 if swarm_count > 0. Confirmed: MOV byte[EBX],0x0 → *more_to_release
 * = 0. */
int ai_release_inactive_swarms(int result_description, char *more_to_release)
{
  char iter[0x1c]; /* extended AI actor iterator; fields through iter+0x18 */
  int record;
  short swarm_count;

  swarm_count = 0;

  if ((result_description == 0) || (more_to_release == (char *)0x0)) {
    display_assert("result_description && more_to_release",
                   "c:\\halo\\SOURCE\\ai\\ai.c", 0x1f7, 1);
    system_exit(-1);
  }

  actor_iterator_new(iter, 0);
  record = actor_iterator_next(iter);
  while (record != 0) {
    if ((*(char *)(record + 6) != '\0') && (*(char *)(record + 8) == '\0') &&
        (*(int *)(record + 0xc) != -1)) {
      swarm_count = (short)(swarm_count + *(short *)(record + 0x1e));
      actor_erase(*(int *)(iter + 0x14), 1);
    }
    record = actor_iterator_next(iter);
  }

  crt_sprintf((char *)result_description, "%d swarm units", (int)swarm_count);
  *more_to_release = 0;
  return swarm_count > 0;
}

/* Compare two AI records for sorting. Primary key: int at offset 8 (ascending).
   Secondary key: unsigned byte at offset 0 (ascending).
   Returns 1 if param_1 < param_2, -1 if param_1 > param_2, 0 if equal. */
int sub_3FB00(unsigned char *param_1, unsigned char *param_2)
{
  if (*(int *)(param_2 + 8) < *(int *)(param_1 + 8)) {
    return 1;
  }
  if (*(int *)(param_2 + 8) > *(int *)(param_1 + 8)) {
    return 0xffffffff;
  }
  if (*param_2 < *param_1) {
    return 0xffffffff;
  }
  return (*param_1 < *param_2);
}

/* ai_find_inactive_encounters (0x3fb40): build the "potentially releasable
 * storage" list — every clump actor and every encounter that is currently
 * inactive — into the caller's working memory, then sort it.
 *
 * Confirmed (XBE 0x3fb40): PUSH EBP / MOV EBP,ESP / SUB ESP,0x18.  Two stack
 *   parameters: [EBP+8] is the storage pointer (ESI, a 16-bit-indexed base)
 *   and [EBP+0xc] is read as a 16-bit unsigned size (CMP word ptr [EBP+0xc],
 *   0xc04 / JNC), so the declaration carries a 2-byte size, not an int.
 *   0xc04 == 4 + 256*12 == sizeof(struct potentially_releasable_storage):
 *   an int16 count at +0, two pad bytes at +2, then up to 0x100 records of
 *   12 bytes at +4 (byte kind at +0, dword at +4, dword handle at +8).
 * Confirmed: the 24 bytes of frame are two 3-dword iterators, not scalars —
 *   [EBP-0xc] is the actor iterator handed to encounter_actor_iterator_new/
 *   _next, so Ghidra's "local_c" ([EBP-0x8]) is actor_iter[1], the iterator's
 *   own current-handle field, and [EBP-0x18] is the encounter iterator.
 *   actor_iter[1] is read in BOTH loops (0x3fbc4 and 0x3fc3e); in the second
 *   loop it is the stale value left by the last actor step, and that is
 *   reproduced here deliberately.
 * Confirmed: EDI holds -1 for the whole body — it is system_exit's argument,
 *   encounter_actor_iterator_new's clump argument, and the sentinel compared
 *   against actor+0xc and encounter+0x10.
 * Confirmed: the count is re-read from memory (MOVSX ECX,word ptr [ESI]) before
 *   each of the three record stores, so it is indexed through the storage
 *   pointer each time rather than cached in a local.
 * Confirmed: the count guard is a signed 16-bit compare against 0x100
 *   (MOV CX,word ptr [ESI] / CMP CX,0x100 / JGE) evaluated after the iterator
 *   step, i.e. the loop tests "more elements AND room left".
 *
 * Call-site verification (cdecl, first PUSH is the last argument):
 *   0x3fb64  PUSH 1 / PUSH 0x22e / PUSH 0x2575c0 / PUSH 0x257648
 *            -> display_assert("working_memory_size >= sizeof(struct "
 *               "potentially_releasable_storage)",
 * "c:\\halo\\SOURCE\\ai\\ai.c", 0x22e, 1) [match] 0x3fb6a  PUSH EDI(-1) ->
 * system_exit(-1)                           [match] 0x3fb85  PUSH EDI(-1) /
 * PUSH EAX(LEA [EBP-0xc])
 *            -> encounter_actor_iterator_new(actor_iter, -1)           [match]
 *   0x3fb8e / 0x3fbe2  PUSH LEA [EBP-0xc]
 *            -> encounter_actor_iterator_next(actor_iter).  ADD ESP,0xc at
 *            0x3fb93 folds this call's 1 dword with iterator_new's 2 — the
 *            ARG_COUNT hazard on that site is a folded-cleanup false positive.
 *   0x3fbf4  PUSH 0 / PUSH ECX(LEA [EBP-0x18])
 *            -> encounter_iterator_new((int)encounter_iter, 0)         [match]
 *   0x3fbfd / 0x3fc59  PUSH LEA [EBP-0x18] ->
 * encounter_iterator_next((int)encounter_iter). ADD ESP,0xc at 0x3fc02 again folds the
 * preceding new(2)+next(1). 0x3fc7c  PUSH 0x3fb00 / PUSH 0xc / PUSH MOVSX
 * EDX,AX(count) / PUSH ESI+4 -> qsort(records, count, 12, sub_3FB00) [match]
 *
 * Uncertain: actor+8, actor+0xc, encounter+0xd, encounter+0x2a and
 * encounter+0x10 are left as raw offsets — only their widths and the sense of
 * each test are proven here. */
void ai_find_inactive_encounters(void *working_memory,
                                 unsigned short working_memory_size)
{
  int actor_iter[3];
  int encounter_iter[3];
  short *storage;
  int actor;
  void *encounter;

  if (working_memory_size < 0xc04) {
    display_assert(
      "working_memory_size >= sizeof(struct potentially_releasable_storage)",
      "c:\\halo\\SOURCE\\ai\\ai.c", 0x22e, 1);
    system_exit(-1);
  }

  storage = (short *)working_memory;
  storage[0] = 0;
  storage[1] = 0;

  encounter_actor_iterator_new(actor_iter, -1);
  actor = encounter_actor_iterator_next(actor_iter);
  while ((actor != 0) && (storage[0] < 0x100)) {
    if ((*(char *)(actor + 8) == '\0') && (*(int *)(actor + 0xc) != -1)) {
      *(char *)(storage + storage[0] * 6 + 2) = 1;
      *(int *)(storage + storage[0] * 6 + 4) = actor_iter[1];
      *(int *)(storage + (storage[0] + 1) * 6) = *(int *)(actor + 0xc);
      storage[0] = (short)(storage[0] + 1);
    }
    actor = encounter_actor_iterator_next(actor_iter);
  }

  encounter_iterator_new((int)encounter_iter, 0);
  encounter = encounter_iterator_next((int)encounter_iter);
  while ((encounter != (void *)0x0) && (storage[0] < 0x100)) {
    if ((*((char *)encounter + 0xd) == '\0') &&
        (*(short *)((char *)encounter + 0x2a) > 0) &&
        (*(int *)((char *)encounter + 0x10) != -1)) {
      *(char *)(storage + storage[0] * 6 + 2) = 0;
      *(int *)(storage + storage[0] * 6 + 4) = actor_iter[1];
      *(int *)(storage + (storage[0] + 1) * 6) =
        *(int *)((char *)encounter + 0x10);
      storage[0] = (short)(storage[0] + 1);
    }
    encounter = encounter_iterator_next((int)encounter_iter);
  }

  if (storage[0] > 0) {
    qsort(storage + 2, (size_t)(int)storage[0], 0xc,
          (qsort_compar_proc)sub_3FB00);
  }
}

/* ai_release_inactive_encounters (0x3fc90): release ONE entry from the
 * "potentially releasable storage" list built by ai_find_inactive_encounters,
 * describe it into result_description, and report whether more entries remain.
 *
 * Confirmed (XBE 0x3fc90): PUSH EBP / MOV EBP,ESP, no SUB ESP — four stack
 *   parameters and no locals.  [EBP+8] result_description, [EBP+0xc]
 *   more_to_release (byte store MOV byte ptr [EDX],CL at 0x3fdb8), [EBP+0x10]
 *   the storage pointer (EDI), and [EBP+0x14] read as a 16-bit UNSIGNED size
 *   (CMP word ptr [EBP+0x14],0xc04 / JNC), so the declaration carries a 2-byte
 *   size, not an int.  Returns a bool in AL (XOR BL,BL at entry, MOV BL,0x1
 *   only on the release path, MOV AL,BL at 0x3fdb6) — the kb decl's
 *   "void (void)" was Ghidra's undetected-parameter default, not the ABI.
 * Confirmed storage layout (same as ai_find_inactive_encounters): int16 count
 *   at +0, int16 cursor at +2, then 12-byte records at +4 (byte kind at +0,
 *   dword handle at +4, dword sort key at +8).  0x3fcfd MOVSX EAX,AX /
 *   LEA EAX,[EAX+EAX*2] / LEA ESI,[EDI+EAX*4+4] == storage + cursor*12 + 4.
 * Confirmed the guard is a SIGNED 16-bit compare of cursor against count
 *   (CMP AX,word ptr [EDI] / JGE at 0x3fcf7, and SETL CL at 0x3fdb2), and the
 *   count/cursor pair is RE-READ from memory at 0x3fda8 after the increment.
 * Confirmed kind sense: CMP byte ptr [ESI],0x0 / JZ 0x3fd4b — the fall-through
 *   (nonzero kind) is the actor case, matching ai_find_inactive_encounters,
 *   which stores kind 1 for clump actors and kind 0 for encounters.
 *
 * Call-site verification (cdecl, first PUSH is the last argument):
 *   0x3fcb6  PUSH 1 / PUSH 0x270 / PUSH 0x2575c0 / PUSH 0x257620
 *            -> display_assert("result_description && more_to_release",
 *               "c:\\halo\\SOURCE\\ai\\ai.c", 0x270, 1)                [match]
 *   0x3fcde  PUSH 1 / PUSH 0x271 / PUSH 0x2575c0 / PUSH 0x257648      [match]
 *   0x3fcbd / 0x3fce5  PUSH -1 -> system_exit(-1)                     [match]
 *   0x3fd18  PUSH ECX([ESI+4]) / PUSH EDX([0x6325a4])
 *            -> datum_get(actor pool, record handle)                  [match]
 *   0x3fd21  PUSH EAX([datum+0x5c]) -> tag_get_name(tag index)        [match]
 *   0x3fd36  PUSH EAX(stripped name) / PUSH 0x2576a8 / PUSH [EBP+8]
 *            -> crt_sprintf(result_description, "encounterless-actor %s", ...)
 *   0x3fd41  PUSH 1 / PUSH EDX([ESI+4]) -> actor_erase(handle, 1).  ADD ESP,
 *            0x24 at 0x3fd46 folds datum_get(2)+tag_get_name(1)+
 *            tag_name_strip_path(1)+crt_sprintf(3)+actor_erase(2) = 9 dwords,
 *            so the ARG_COUNT hazard on that site is a folded-cleanup false
 *            positive.
 *   0x3fd59  PUSH 0xb0 / PUSH EAX(handle & 0xffff), then CALL
 *            global_scenario_get and PUSH EAX+0x42c at 0x3fd63
 *            -> tag_block_get_element(scenario+0x42c, index, 0xb0)    [match]
 *            The element pointer is parked in EBX across the following
 *            datum_get, so the two lookups are sequenced here with temporaries
 *            rather than nested in the crt_sprintf argument list.
 *   0x3fd76  PUSH ECX([ESI+4]) / PUSH EDX([0x5ab270])
 *            -> datum_get(encounter pool, record handle); MOVSX from +0x2a is
 *            the encounter's live-unit count printed as %d.            [match]
 *   0x3fd99  PUSH 1 / PUSH -1 / PUSH -1 / PUSH EDX([ESI+4])
 *            -> ai_erase(handle, -1, -1, 1).  ADD ESP,0x34 at 0x3fd9e folds
 *            global_scenario_get's 2 + tag_block_get_element(1) +
 *            datum_get(2) + crt_sprintf(4) + ai_erase(4) = 13 dwords.
 *
 * Uncertain: the encounter record's +0x2a short and the actor record's +0x5c
 * dword are left as raw offsets — only their widths and their roles as a unit
 * count and a tag index are proven here.  The scenario block at +0x42c has a
 * 0xb0-byte element stride whose fields are not needed by this function. */
bool ai_release_inactive_encounters(char *result_description,
                                    bool *more_to_release, void *working_memory,
                                    unsigned short working_memory_size)
{
  short *storage;
  short *record;
  void *encounter_name;
  short unit_count;
  const char *actor_tag_name;
  bool released;

  released = 0;

  if ((result_description == (char *)0x0) || (more_to_release == (bool *)0x0)) {
    display_assert("result_description && more_to_release",
                   "c:\\halo\\SOURCE\\ai\\ai.c", 0x270, 1);
    system_exit(-1);
  }

  if (working_memory_size < 0xc04) {
    display_assert(
      "working_memory_size >= sizeof(struct potentially_releasable_storage)",
      "c:\\halo\\SOURCE\\ai\\ai.c", 0x271, 1);
    system_exit(-1);
  }

  storage = (short *)working_memory;
  if (storage[1] < storage[0]) {
    record = storage + storage[1] * 6 + 2;
    if (*(char *)record != '\0') {
      actor_tag_name = tag_get_name(
        *(int *)((char *)datum_get(*(data_t **)0x6325a4, *(int *)(record + 2)) +
                 0x5c));
      crt_sprintf(result_description, "encounterless-actor %s",
                  tag_name_strip_path(actor_tag_name));
      actor_erase(*(int *)(record + 2), 1);
    } else {
      encounter_name = tag_block_get_element(
        (char *)global_scenario_get() + 0x42c,
        (int)(*(unsigned int *)(record + 2) & 0xffff), 0xb0);
      unit_count = *(
        short *)((char *)datum_get(*(data_t **)0x5ab270, *(int *)(record + 2)) +
                 0x2a);
      crt_sprintf(result_description, "encounter %s (%d units)", encounter_name,
                  (int)unit_count);
      ai_erase(*(int *)(record + 2), -1, -1, 1);
    }
    storage[1] = (short)(storage[1] + 1);
    released = 1;
  }

  *more_to_release = storage[1] < storage[0];
  return released;
}

/* ai_handle_unit_approach: test whether a unit is approaching a valid
 * target for an AI actor, and optionally record the approach.
 * Looks up the actor via actor_data, checks the unit against
 * object_get_and_verify_type (type 3 = unit), tests team friendship via
 * game_allegiance_get_team_is_friendly, and returns true (1) when the
 * teams are NOT friendly (i.e. the unit is an enemy worth approaching).
 * If flag is non-zero and the check passes, sets the approach-active
 * flag at actor+0x2ed by calling actor_stimulus_vehicle_eviction.
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
      if (!game_allegiance_get_team_is_friendly(
            *(int16_t *)(unit + 0x68), ((actor_t *)actor)->field_03e)) {
        result = 1;
        if (flag) {
          /* set the approach-active flag at actor+0x2ed */
          actor_stimulus_vehicle_eviction(ai_handle);
        }
      }
    }
  }
  return result;
}

/* ai_get_responsible_unit: resolve a vehicle unit handle to an occupant handle.
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
int ai_get_responsible_unit(int unit_handle, char prefer_passenger)
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

/* ai_handle_death: Notify AI systems when a unit exits a vehicle.
 *
 * param_1 (unit_handle): the unit that just exited.
 * param_2: passed to ai_get_responsible_unit as unit_handle for
 * vehicle-occupant resolution. param_3: vehicle/context handle; used to control
 * prefer_passenger (word !=9) and forwarded as param5 of
 * ai_communication_event.
 *
 * Resolves the occupant from param_2 via ai_get_responsible_unit, determines a
 * relationship code (0=same unit, 2=enemy, 3=friendly, or 0xffffffff if no
 * valid occupant), then notifies the AI communication system
 * (ai_communication_event), clears encounter references
 * (ai_conversation_unit_died), and updates encounter kill counts
 * (encounters_unit_died).
 *
 * Confirmed: [EBP+8]=unit_handle (int), [EBP+C]=param_2 (int),
 *            [EBP+10]=param_3 compared as word ptr. */
void ai_handle_death(int unit_handle, int param_2, short param_3)
{
  int relation;
  int resolved;
  void *obj_unit;
  void *obj_resolved;

  resolved = ai_get_responsible_unit(param_2, (char)(param_3 != 9));
  relation = 0;
  if (unit_handle == resolved) {
    relation = 1;
  } else if (resolved != -1) {
    obj_unit = object_get_and_verify_type(unit_handle, 3);
    obj_resolved = object_get_and_verify_type(resolved, 3);
    relation = (game_allegiance_get_team_is_friendly(
                  *(short *)((char *)obj_unit + 0x68),
                  *(short *)((char *)obj_resolved + 0x68)) != 0) +
               2;
  }
  ai_communication_event(0, unit_handle, resolved, relation, (int)param_3, -1,
                         0);
  ai_conversation_unit_died(unit_handle, '\0');
  encounters_unit_died(unit_handle);
}

/*
 * ai_handle_killing_spree: AI killing spree threshold check and notification.
 *
 * Given a unit handle and a killing spree count, checks whether the count
 * meets the threshold to trigger a killing-spree AI communication event.
 * Threshold is 3 if the unit has no rider (unit+0x1c8 == 0xffffffff), or
 * 5 if it does. When the debug flag at 0x5aca60 is set, logs the spree count
 * to the console. If the threshold is met, fires ai_communication_event with
 * type=1 and returns 1; otherwise returns 0.
 *
 * Confirmed: [EBP+8]=unit_handle (int), [EBP+C]=killing_spree_count (short),
 *            threshold = (uVar1 != 0xffffffff)*2 + 3 = 3 (no rider) or 5
 * (rider).
 */
char ai_handle_killing_spree(int unit_handle, short killing_spree_count)
{
  char buf[512];
  void *obj;
  unsigned int rider;
  short threshold;

  obj = object_get_and_verify_type(unit_handle, 3);
  rider = *(unsigned int *)((char *)obj + 0x1c8);
  threshold = (short)((rider != 0xffffffffu) * 2 + 3);

  if (*(char *)0x5aca60 != '\0') {
    if (rider == 0xffffffffu) {
      ai_debug_describe_actor(*(int *)((char *)obj + 0x1a4), unit_handle, 1,
                              buf, 0x200);
    } else {
      crt_sprintf(buf, "player%d", (unsigned int)(rider & 0xffff));
    }
    console_printf(0, "%s killing spree: %d", buf, (int)killing_spree_count);
  }

  if (killing_spree_count >= threshold) {
    ai_communication_event(1, unit_handle, -1, -1, -1, -1, 0);
    return 1;
  }
  return 0;
}

/* game_allegiance_apply_change: apply an allegiance change between two
 * teams, updating all matching actor records in the AI actor iterator.
 * Iterates over all active player-actors via
 * actor_iterator_new/actor_iterator_next; for each actor whose team
 * matches team_a or team_b, walks the actor's clump items via
 * prop_iterator_new/prop_iterator_next and applies the friendship and force flags.
 * Confirmed: 4 args via PUSH count
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
  char iter[0x1c]; /* extended AI actor iterator; see actor_iterator_new */
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
  actor_iterator_new(iter, 1);
  actor = actor_iterator_next(iter);
  while (actor != 0) {
    /* check if this actor belongs to team_a or team_b */
    matched_team = team_b;
    if (((actor_t *)actor)->field_03e == team_a) {
      matched_team = team_b;
    } else if (((actor_t *)actor)->field_03e == team_b) {
      matched_team = team_a;
    } else {
      goto next_actor;
    }
    if (matched_team == -1) {
      goto next_actor;
    }

    /* walk this actor's clump items, using actor handle from iter.field_0x14 */
    prop_iterator_new(clump_iter, *(int *)(iter + 0x14));
    clump_item = prop_iterator_next(clump_iter);
    while (clump_item != 0) {
      if (*(int16_t *)(clump_item + 0x12) == matched_team) {
        if (!force) {
          /* mark clump item fields +0x61 and +0x62 */
          *(char *)(clump_item + 0x61) = 1;
          *(char *)(clump_item + 0x62) = 1;
        }
        if (!friendship || force) {
          *(char *)(clump_item + 0x60) = friendship;
          *(char *)(clump_item + 0xa4) = actor_compute_prop_unopposable(
            *(int *)(iter + 0x14), clump_iter[0]);
          *(float *)(clump_item + 0x50) = actor_compute_prop_target_weight(
            *(int *)(iter + 0x14), clump_iter[0]);
        }
      }
      clump_item = prop_iterator_next(clump_iter);
    }

  next_actor:
    actor = actor_iterator_next(iter);
  }
}

/* 0x40150 — ai_handle_allegiance_broken_notification.
 * Propagate an allegiance break/reform between two teams to every active AI
 * actor that belongs to one of the two teams, then notify the game layer.
 *
 * Params (cdecl, stack): [EBP+0x8] team_a (int16_t, MOVSX at the name-table
 * index and CMP AX,DI), [EBP+0xc] team_b (int16_t), [EBP+0x10] broken (char,
 * MOV AL,byte ptr).
 *
 * Debug print (only when *(char *)0x5aca55): the message string is selected
 * before the table loads — EAX = 0x257718 "broken" by default, replaced with
 * 0x25770c "reformed" when the broken byte is zero.  cdecl push order at
 * 0x40190 is PUSH EAX(state) / PUSH ECX(team_b name) / PUSH EAX(team_a name) /
 * PUSH fmt / PUSH 0, i.e. console_printf(0, fmt, names[team_a], names[team_b],
 * state); ADD ESP,0x14 confirms 5 stack args.
 *
 * Stack layout (EBP-based, SUB ESP,0x24) — same shape as
 * game_allegiance_apply_change at 0x40010:
 *   [EBP-0x24..EBP-0x09]: ai_actor_iter (0x1c bytes extended iterator)
 *   [EBP-0x08]:           clump_iter[0] (current clump-item handle)
 *   [EBP-0x04]:           clump_iter[1] (next clump-item handle)
 * [EBP-0x10] is ai_actor_iter+0x14 (current actor handle), which the
 * decompiler names 'local_14' because it overlaps the iterator buffer.
 *
 * Matched-team selection (0x401c0): EDI is preloaded with team_a; when the
 * actor's field_03e equals team_a the code reloads EDI with team_b, otherwise
 * it compares against team_b and keeps team_a.  So matched_team is always the
 * *other* team, and -1 skips the actor.
 *
 * Inner stores (0x40206): +0x61 = 1, +0x62 = 0 (BL, the zero register),
 * +0x60 = broken, +0xa4 = actor_compute_prop_unopposable (byte from AL),
 * +0x50 = actor_compute_prop_target_weight (FSTP float).  Both calls take
 * (actor handle from iter+0x14, clump_iter[0]) — PUSH ECX([EBP-0x8]) then
 * PUSH EDX([EBP-0x10]), so the handle is the first argument.
 *
 * Tail call at 0x40269: PUSH [EBP+0xc] then PUSH [EBP+0x8] →
 * game_allegiance_notify_change(team_a, team_b). */
void ai_handle_allegiance_broken_notification(int16_t team_a, int16_t team_b,
                                              char broken)
{
  char iter[0x1c]; /* extended AI actor iterator; see actor_iterator_new */
  int clump_iter[2]; /* clump-item walk: [0]=current handle, [1]=next */
  int16_t matched_team;
  int actor;
  int clump_item;

  /* optional debug console print */
  if (*(char *)0x5aca55) {
    const char *state = broken ? "broken" : "reformed";
    console_printf(0, "allegiance between teams %s and %s communicated as %s",
                   ((const char **)0x2efdf8)[team_a],
                   ((const char **)0x2efdf8)[team_b], state);
  }

  /* initialise iterator over all active player-actors (flag=1) */
  actor_iterator_new(iter, 1);
  actor = actor_iterator_next(iter);
  while (actor != 0) {
    /* matched_team is the opposite team of the one this actor belongs to */
    matched_team = team_b;
    if (((actor_t *)actor)->field_03e == team_a) {
      matched_team = team_b;
    } else if (((actor_t *)actor)->field_03e == team_b) {
      matched_team = team_a;
    } else {
      goto next_actor;
    }
    if (matched_team == -1) {
      goto next_actor;
    }

    /* walk this actor's clump items, using actor handle from iter.field_0x14 */
    prop_iterator_new(clump_iter, *(int *)(iter + 0x14));
    clump_item = prop_iterator_next(clump_iter);
    while (clump_item != 0) {
      if (*(int16_t *)(clump_item + 0x12) == matched_team) {
        *(char *)(clump_item + 0x61) = 1;
        *(char *)(clump_item + 0x62) = 0;
        *(char *)(clump_item + 0x60) = broken;
        *(char *)(clump_item + 0xa4) =
          actor_compute_prop_unopposable(*(int *)(iter + 0x14), clump_iter[0]);
        *(float *)(clump_item + 0x50) = actor_compute_prop_target_weight(
          *(int *)(iter + 0x14), clump_iter[0]);
      }
      clump_item = prop_iterator_next(clump_iter);
    }

  next_actor:
    actor = actor_iterator_next(iter);
  }

  game_allegiance_notify_change(team_a, team_b);
}

/* 0x40280 — Update clump-item perception fields for all active AI actors.
 * Iterates all active actors; for each actor's clump items, retrieves the
 * unit's team, checks friendliness/hostility against the actor's team,
 * and computes perception visibility and distance. */
void ai_update_team_status(void)
{
  char iter[0x1c];
  int clump_iter[2];
  int actor;
  int clump_item;
  int unit;
  short team;

  actor_iterator_new(iter, 1);
  actor = actor_iterator_next(iter);
  while (actor != 0) {
    prop_iterator_new(clump_iter, *(int *)(iter + 0x14));
    clump_item = prop_iterator_next(clump_iter);
    while (clump_item != 0) {
      unit = (int)object_get_and_verify_type(*(int *)(clump_item + 0x18), 3);
      team = *(short *)(unit + 0x68);
      *(short *)(clump_item + 0x12) = team;
      *(char *)(clump_item + 0x60) = game_allegiance_get_team_is_friendly(
        ((actor_t *)actor)->field_03e, (int)team);
      *(char *)(clump_item + 0x61) = game_team_is_ally(
        ((actor_t *)actor)->field_03e, *(short *)(clump_item + 0x12));
      *(char *)(clump_item + 0xa4) =
        actor_compute_prop_unopposable(*(int *)(iter + 0x14), clump_iter[0]);
      *(float *)(clump_item + 0x50) =
        actor_compute_prop_target_weight(*(int *)(iter + 0x14), clump_iter[0]);
      clump_item = prop_iterator_next(clump_iter);
    }
    actor = actor_iterator_next(iter);
  }
}

/*
 * ai_handle_bump: Remove AI encounter relationships between two units.
 *
 * param_1: actor/unit handle (the acting unit; provides the encounter via
 * +0x1a4). param_2: vehicle or unit handle to resolve; if the resolved unit has
 * a driver (unit+0x2d4 != -1), the driver handle replaces param_2.
 *
 * Conditions that skip the encounter removal:
 *   - param_2 == -1 or try_and_get resolves NULL.
 *   - The resolved handle is still -1 after driver promotion.
 *   - game_connection() == 0 AND DAT_005ac9c6 != '\0' AND rider (unit+0x1c8) !=
 * -1.
 *   - word at (resolved_unit + 0x64) != 0.
 *
 * When all conditions pass, calls prop_get_base_by_unit_index to look up the slot index,
 * then actor_handle_unit_effect(encounter_handle, slot_index, 0) on both
 * directions (param_1's encounter vs param_2, and param_2's encounter vs
 * param_1).
 *
 * Confirmed: [EBP+8]=param_1 (int), [EBP+C]=param_2 (int),
 * [EBP+10]=velocity_ptr (ignored). The third arg is pushed by biped_bumped_object but
 * never accessed by this function.
 */
void ai_handle_bump(int param_1, int param_2, float *velocity_ptr)
{
  void *obj2;
  void *obj1;
  int slot;
  int enc;

  if (param_2 == -1) {
    return;
  }
  obj2 = object_try_and_get_and_verify_type(param_2, 3);
  if (obj2 == NULL) {
    return;
  }
  /* promote param_2 to driver if present */
  if (*(int *)((char *)obj2 + 0x2d4) != -1) {
    param_2 = *(int *)((char *)obj2 + 0x2d4);
  }
  if (param_2 == -1) {
    return;
  }
  /* network + rider guard: skip if standalone + rider occupied */
  if (game_connection() == 0 && *(char *)0x5ac9c6 != '\0') {
    obj2 = object_get_and_verify_type(param_2, 3);
    if (*(int *)((char *)obj2 + 0x1c8) != -1) {
      return;
    }
  }
  /* skip if field_64 word is non-zero */
  obj2 = object_get_and_verify_type(param_2, 3);
  if (*(short *)((char *)obj2 + 0x64) != 0) {
    return;
  }

  /* remove param_1's encounter entry for param_2 */
  obj1 = object_get_and_verify_type(param_1, 3);
  enc = *(int *)((char *)obj1 + 0x1a4);
  if (enc != -1) {
    slot = prop_get_base_by_unit_index(enc, param_2, 1, 0);
    if (slot != -1) {
      actor_handle_unit_effect(*(int *)((char *)obj1 + 0x1a4), slot, 0);
    }
  }

  /* remove param_2's encounter entry for param_1 */
  enc = *(int *)((char *)obj2 + 0x1a4);
  if (enc != -1) {
    slot = prop_get_base_by_unit_index(enc, param_1, 1, 0);
    if (slot != -1) {
      actor_handle_unit_effect(*(int *)((char *)obj2 + 0x1a4), slot, 0);
    }
  }
}

/* ai_handle_damage: notify the AI systems that a unit took damage.
 *
 * unit_handle ([EBP+8]): the damaged unit. param_2 ([EBP+C]): the damaging
 * unit/vehicle handle, resolved to a responsible occupant via
 * ai_get_responsible_unit. param_3 ([EBP+10]): damage-source category,
 * compared as int16_t (CMP BX,9 / CMP BX,1) and forwarded as param5 of
 * ai_communication_event. damage ([EBP+14]): damage amount (float; FLD
 * [EBP+0x14] / FCOMP [0x2533e4] and forwarded as actor_handle_damage param_3).
 * param_5 ([EBP+18]): forwarded to actor_handle_damage param_4.
 * param_6 ([EBP+1C]): char flag; when non-zero suppresses both the actor
 * damage notification and the "betrayal" (type 3) communication event.
 *
 * Relationship code: 1 = damaged itself, 2/3 from
 * game_allegiance_get_team_is_friendly (2 = enemy, 3 = friendly), 0 when the
 * responsible unit could not be resolved.
 *
 * Confirmed: 6 stack params, ADD ESP cleanups 0x10/0x8/0x10/0x8/0x1c/0x8.
 * Confirmed: object_get_and_verify_type(handle, 3); actor handle at obj+0x1a4;
 *   team at obj+0x68 (int16_t).
 * Confirmed: friendly-check arg order PUSH EAX(resolved team) then
 *   PUSH EDX(unit team) => (unit_team, resolved_team); provoke pushes are
 *   reversed => (resolved_team, unit_team).
 * Confirmed: FCOMP + TEST AH,1 + JNZ skip => call when damage >= threshold. */
void ai_handle_damage(int unit_handle, int param_2, int param_3, float damage,
                      int param_5, char param_6)
{
  void *unit_obj;
  void *resolved_obj;
  int resolved;
  int actor_handle;
  int relation;

  unit_obj = object_get_and_verify_type(unit_handle, 3);
  resolved = ai_get_responsible_unit(param_2, (char)((short)param_3 != 9));
  if (resolved == -1) {
    resolved_obj = (void *)0;
  } else {
    resolved_obj = object_get_and_verify_type(resolved, 3);
  }

  if (param_6 == '\0' && (short)param_3 != 1) {
    actor_handle = *(int *)((char *)unit_obj + 0x1a4);
    if (actor_handle != -1) {
      actor_handle_damage(actor_handle, resolved, damage, param_5);
    }
  }

  relation = 0;
  if (unit_handle == resolved) {
    relation = 1;
  } else if (resolved_obj != (void *)0) {
    relation = (game_allegiance_get_team_is_friendly(
                  *(short *)((char *)unit_obj + 0x68),
                  *(short *)((char *)resolved_obj + 0x68)) != 0) +
               2;
  }

  if (param_6 == '\0' && (short)relation == 2) {
    ai_communication_event(3, unit_handle, resolved, 2, param_3, -1, 0);
  } else if (damage >= *(float *)0x2533e4) {
    ai_communication_event(2, unit_handle, resolved, relation, param_3, -1, 0);
  }

  if (resolved_obj != (void *)0) {
    game_allegiance_provoke(*(short *)((char *)resolved_obj + 0x68),
                            *(short *)((char *)unit_obj + 0x68));
  }
}

/* ai_place_pending_mounted_weapons: spawn AI actors into vehicle seats from pending vehicle list.
 * Called each tick from ai_update. Iterates the vehicle spawn queue stored
 * in the AI globals block: a count at offset +0x8b8 (int16_t) and an array
 * of object handles starting at offset +0x8bc. For each queued vehicle,
 * looks up its unit tag definition and walks the tag_block at tag+0x2e4
 * (element size 0x11c). For each seat element with a valid actor variant
 * tag index at element+0x104, creates an actor via actor_place using the
 * vehicle's world position as the starting location, then boards the new
 * actor's unit into the vehicle at the corresponding seat index.
 * Clears the queue count to zero after processing.
 *
 * Confirmed: void(void) — no args, no return value.
 * Confirmed: outer loop uses CMP SI (16-bit comparison).
 * Confirmed: inner loop counter sign-extended via MOVSX EAX,AX.
 * Confirmed: csmemset size 0x1c, word at buffer+0x1a = 0xffff.
 * Confirmed: actor_place args: 6 pushes, ADD ESP,0x2c (cleans 6 args + prior
 * 5). Confirmed: unit_board_vehicle args: PUSH EDX(seat), PUSH EDI(vehicle),
 * PUSH EAX(unit). */
void ai_place_pending_mounted_weapons(void)
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
        actor_handle = actor_place(*(int *)(seat_element + 0x104), -1, -1,
                                    starting_location, 0, 0);
        if (actor_handle != -1) {
          actor = datum_get(actor_data, actor_handle);
          unit_board_vehicle(((actor_t *)actor)->field_018, vehicle_handle, j);
        }
      }
    }
  }

  *(int16_t *)(*(volatile int *)0x632574 + 0x8b8) = 0;
}

/* ai_create_mounted_weapons_for_unit: enqueue a vehicle unit handle into the AI
 * mounted-weapon pending spawn list. Checks the AI-initialized guard at
 * globals+0x1, then appends param_1 to the array at globals+0x8bc (capacity 8,
 * count int16_t at globals+0x8b8) if there is room. If the list is full, logs a
 * warning via error(). Called from unit_new (one caller).
 *
 * Confirmed: one stack param [EBP+8], no return value.
 * Confirmed: CMP AX,0x8; MOVSX EAX,AX before indexed store.
 * Confirmed: object_get_and_verify_type(param_1, 3) → *(ptr) → tag_get_name →
 *   tag_name_strip_path → error(2, warning_str, name) when list is full. */
void ai_create_mounted_weapons_for_unit(int param_1)
{
  int g;
  void *unit_obj;
  int tag_index;
  const char *tag_name;
  const char *stripped;

  g = *(volatile int *)0x632574;
  if (*(char *)(g + 1) != '\0') {
    if (*(int16_t *)(g + 0x8b8) < 8) {
      *(int *)(g + 0x8bc + (int)(*(int16_t *)(g + 0x8b8)) * 4) = param_1;
      g = *(volatile int *)0x632574;
      *(int16_t *)(g + 0x8b8) += 1;
      return;
    }
    unit_obj = object_get_and_verify_type(param_1, 3);
    tag_index = *(int *)unit_obj;
    tag_name = tag_get_name(tag_index);
    stripped = tag_name_strip_path(tag_name);
    error(2,
          "WARNING: cannot create mounted weapons for %s, exceeded "
          "MAXIMUM_NUMBER_OF_MOUNTED_WEAPON_UNITS",
          stripped);
  }
}

/* 0x40860 — ai_handle_unit_effect: broadcast an AI unit effect (sound/visual
 * cue) from a unit to its AI actor(s), rate-limited per unit.
 *
 * Confirmed: gated on byte[ai_globals+1] (AI active); early RET when clear.
 * Confirmed: two asserts at ai.c:0x729 (volume 0..4) and 0x72a (effect_type
 * 0..3), each display_assert(...,1) + system_exit(-1). The 16-bit compares
 * (TEST SI,SI / CMP SI,0x5) prove both params are truncated to short.
 * Confirmed: guards unit_handle != -1 and volume > 0 (JLE) before any call.
 * Confirmed: call order object_get_and_verify_type(unit_handle,3) -> ESI,
 * game_time_get() -> EDI, game_connection() -> AX.
 * Confirmed: network/rider guard — returns only when game_connection()==0 AND
 * byte[0x5ac9c6]!=0 AND unit+0x1c8 != -1.
 * Confirmed: rate limit — proceeds when effect > word[unit+0x1cc] (CMP BX,
 * [ESI+0x1cc]; JG) or time > int[unit+0x1d0]+0x1e (JLE returns).
 * Confirmed: word[unit+0x64] is read into AX BEFORE the two stores to
 * unit+0x1cc / unit+0x1d0, then tested against 1 and 0.
 * Confirmed: kind==1 walks the child list from int[unit+0xc8], verifying each
 * with object_get_and_verify_type(handle,-1) (type_mask -1), dispatching
 * actors_handle_unit_effect(child_handle, effect, priority) for children whose
 * word+0x64 is 0, advancing via int[child+0xc4] until -1.
 * Confirmed: kind==0 dispatches actors_handle_unit_effect(unit_handle, effect,
 * priority) for the unit itself; any other kind falls through.
 * Confirmed: all calls cdecl (ADD ESP,0x8 / 0xc). No return value. */
void ai_handle_unit_effect(int unit_handle, int effect_type, int priority)
{
  short volume;
  short effect;
  short kind;
  char *unit_obj;
  char *child_obj;
  int time;
  int child_handle;

  if (*(char *)(*(int *)0x632574 + 1) == '\0') {
    return;
  }
  volume = (short)priority;
  if ((volume < 0) || (volume > 4)) {
    display_assert("volume>=0 && volume<NUMBER_OF_AI_SOUND_VOLUMES",
                   "c:\\halo\\SOURCE\\ai\\ai.c", 0x729, 1);
    system_exit(-1);
  }
  effect = (short)effect_type;
  if ((effect < 0) || (effect > 3)) {
    display_assert("effect_type>=0 && effect_type<NUMBER_OF_AI_UNIT_EFFECTS",
                   "c:\\halo\\SOURCE\\ai\\ai.c", 0x72a, 1);
    system_exit(-1);
  }
  if ((unit_handle == -1) || (volume <= 0)) {
    return;
  }
  unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);
  time = game_time_get();
  if (game_connection() == 0 && *(char *)0x5ac9c6 != '\0' &&
      *(int *)(unit_obj + 0x1c8) != -1) {
    return;
  }
  if (effect <= *(short *)(unit_obj + 0x1cc) &&
      time <= *(int *)(unit_obj + 0x1d0) + 0x1e) {
    return;
  }
  kind = *(short *)(unit_obj + 0x64);
  *(short *)(unit_obj + 0x1cc) = effect;
  *(int *)(unit_obj + 0x1d0) = time;
  if (kind == 1) {
    child_handle = *(int *)(unit_obj + 0xc8);
    while (child_handle != -1) {
      child_obj = (char *)object_get_and_verify_type(child_handle, -1);
      if (*(short *)(child_obj + 0x64) == 0) {
        actors_handle_unit_effect(child_handle, effect, priority);
      }
      child_handle = *(int *)(child_obj + 0xc4);
    }
  } else if (kind == 0) {
    actors_handle_unit_effect(unit_handle, effect, priority);
  }
}

/* unit_vehicle_board_notify: notify the AI subsystem that a unit is boarding
 * a vehicle. Verifies the unit object (type_mask=3 for biped|vehicle), and
 * if the unit has an AI actor (offset 0x1a4 != -1), dispatches an AI
 * command via ai_communication_event with command type 0x24.
 * The vehicle_handle parameter is accepted but unused in this function body.
 * Confirmed: 1 stack param used ([EBP+8]), second param ([EBP+0xc]) untouched.
 * Confirmed: PUSH order for ai_communication_event — 7 args, cdecl (ADD
 * ESP,0x1c). */
void unit_vehicle_board_notify(int unit_handle, int vehicle_handle)
{
  void *unit_obj = object_get_and_verify_type(unit_handle, 3);
  if (*(int *)((char *)unit_obj + 0x1a4) != -1) {
    ai_communication_event(0x24, unit_handle, -1, -1, -1, -1, 0);
  }
}

/* ai_handle_exit_vehicle: notify the AI subsystem that a unit is exiting a
 * vehicle. Looks up the unit object (type_mask=3), checks whether the unit has
 * a valid AI actor handle at offset +0x1a4. If the actor exists, retrieves the
 * actor record from actor_data and checks the byte flag at actor+0x38c. If
 * the flag is clear, dispatches AI command 0x25 via ai_communication_event to
 * notify the subsystem of the vehicle-exit event. The flag at actor+0x38c is
 * then cleared unconditionally (whether or not the command was dispatched).
 *
 * Confirmed: 1 stack param [EBP+8] (unit handle). No return value.
 * Confirmed: object_get_and_verify_type(param_1, 3); EAX+0x1a4 = actor handle.
 * Confirmed: datum_get([0x6325a4], actor_handle); result in ESI.
 * Confirmed: TEST AL,AL on [ESI+0x38c]; JNZ skips ai_communication_event call.
 * Confirmed: ai_communication_event(0x25, param_1, -1, -1, -1, -1, 0), 7 args
 * cdecl (ADD ESP,0x1c). MOV byte [ESI+0x38c],0 always executes. */
void ai_handle_exit_vehicle(int param_1)
{
  char *unit_obj;
  int actor_handle;
  char *actor;

  unit_obj = (char *)object_get_and_verify_type(param_1, 3);
  actor_handle = *(int *)(unit_obj + 0x1a4);
  if (actor_handle != -1) {
    actor = (char *)datum_get(actor_data, actor_handle);
    if (((actor_t *)actor)->field_38c == '\0') {
      ai_communication_event(0x25, param_1, -1, -1, -1, -1, 0);
    }
    ((actor_t *)actor)->field_38c = 0;
  }
}

/* ai_flush_spatial_effects: clear the AI encounter/firing-position cache fields in the
 * globals block. Zeroes the int16_t counts at globals+0x130 and globals+0x132,
 * then csmemsets 0x280 bytes starting at globals+0x134 to zero.
 *
 * Confirmed: void(void) — no args, no return value.
 * Confirmed: three stores then CALL csmemset(globals+0x134, 0, 0x280).
 * Confirmed: ADD ESP,0xc (3 args); RET. */
void ai_flush_spatial_effects(void)
{
  int g;

  g = *(volatile int *)0x632574;
  *(int16_t *)(g + 0x132) = 0;
  *(int16_t *)(g + 0x130) = 0;
  csmemset((void *)(g + 0x134), 0, 0x280);
}

/* ai_reconnect_to_structure_bsp: iterate all encounterless actors and re-attach
 * any whose encounter's BSP index matches the current structure BSP.
 *
 * Walks the global encounterless-actor linked list (head at globals+0x8,
 * next-handle at actor+0x2c). For each actor:
 *   - Asserts actor[9] != 0 (encounterless flag must be set).
 *   - Skips actors with no encounter reference (actor+0x30 == -1).
 *   - Resolves the encounter element via global_scenario_get() +
 *     tag_block_get_element(scenario+0x42c, encounter_index, 0xb0).
 *   - If the element's BSP index (element+0x7e) matches the current BSP,
 *     calls encounterless_detach_actor (encounter_leave/detach) then
 * encounter_attach_actor to re-attach the actor to its encounter and squad.
 *
 * Confirmed: void(void) — no args, no return.
 * Confirmed: PUSH 0xb0 + PUSH encounter_idx are pre-staged args for
 *   tag_block_get_element; ADD ESP,0xc cleans all 3 args at once.
 * Confirmed: XOR EAX,EAX; MOV AX,[ESI+0x38] = zero-extend squad index.
 * Confirmed: MOV EBX,[ESI+0x2c] saved before body — next saved early. */
void ai_reconnect_to_structure_bsp(void)
{
  short bsp_index;
  int actor_handle;
  int next_handle;
  char *actor;
  char *scenario;
  char *encounter_element;
  int encounter_ref;
  int g;

  bsp_index = global_structure_bsp_index_get();
  g = *(int *)0x632574;
  actor_handle = *(int *)(g + 0x8);
  while (actor_handle != -1) {
    actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);
    next_handle = ((actor_t *)actor)->field_02c;
    if (((actor_t *)actor)->field_009 == '\0') {
      display_assert("actor->meta.encounterless", "c:\\halo\\SOURCE\\ai\\ai.c",
                     0x96f, 1);
      system_exit(-1);
    }
    encounter_ref = ((actor_t *)actor)->field_030;
    if (encounter_ref != -1) {
      scenario = (char *)global_scenario_get();
      encounter_element = (char *)tag_block_get_element(
        scenario + 0x42c, encounter_ref & 0xffff, 0xb0);
      if (*(short *)(encounter_element + 0x7e) == bsp_index) {
        encounterless_detach_actor(actor_handle);
        encounter_attach_actor(actor_handle, ((actor_t *)actor)->field_030,
                               ((actor_t *)actor)->field_038, 1);
      }
    }
    actor_handle = next_handle;
  }
}

/* ai_get_race_from_team_index: map an actor/encounter type index to a flag/size
 * value. Takes a short type code (1-5) and returns the corresponding constant:
 *   1 -> 1, 2 -> 2, 3 -> 4, 4 -> 0x38, 5 -> 0x40, else 0.
 * Confirmed from disasm at 0x41040: MOV CX,word ptr [EBP+0x8],
 * cdecl short param, returns int via EAX. */
int ai_get_race_from_team_index(short param_1)
{
  int uVar1;

  uVar1 = 0;
  if (param_1 == 1) {
    return 1;
  }
  if (param_1 == 2) {
    return 2;
  }
  if (param_1 == 3) {
    return 4;
  }
  if (param_1 == 4) {
    return 0x38;
  }
  if (param_1 == 5) {
    uVar1 = 0x40;
  }
  return uVar1;
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

  ai_debug_initialize_for_new_map();
  ai_profile_initialize_for_new_map();
  paths_initialize_for_new_map();
  actors_initialize_for_new_map();
  props_initialize_for_new_map();
  encounters_initialize_for_new_map();
  ai_script_initialize_for_new_map();
  ai_communication_initialize_for_new_map();

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

  /* check AI active and not paused */
  if (*(char *)(*(int *)0x632574 + 1) && !*(char *)0x5abaa0) {
    should_update = 1;
  } else {
    should_update = 0;
  }

  /* MATCH-SENSITIVE: the 0x5abaa1 read is duplicated into both arms of the
   * profile_global_enable test on purpose.  The original loads BL between
   * `TEST AL,AL` (0x449ef1) and `TEST AL,AL` (0x2c8738) at 0x411a9, right
   * after PUSH EBX; the split-if is the only C form that reproduces that
   * ordering.  Hoisting the read above or sinking it below the block costs
   * 3.2pp and 9.4pp of VC71 match respectively. */
  if (*(bool *)0x449ef1) {
    schedule_flag = *(char *)0x5abaa1;
    if (*(char *)0x2c8738) {
      profile_enter_private((void *)0x2c8730);
    }
  } else {
    schedule_flag = *(char *)0x5abaa1;
  }

  if (should_update) {
    ai_debug_update();
    ai_profile_update();
    ai_place_pending_mounted_weapons();
    if (schedule_flag) {
      /* scripted/scheduled actor branch */
      actors_move_randomly();
      *(char *)(*(int *)0x632574 + 2) = 1;
    } else {
      if (*(char *)*(int *)0x632574) {
        /* first-frame / map-load branch */
        ai_conversation_update();
        encounters_update();
        actors_update();
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
    profile_exit_private((void *)0x2c8730);
  }
}

/* ai_generate_line_of_fire_pill: fill one ai_firing_pos_entry_t in the candidate buffer.
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
void ai_generate_line_of_fire_pill(ai_firing_pos_entry_t *entry, int unit_handle,
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

/* ai_find_line_of_fire_friend_pills: build the firing-position candidate list for an actor.
 *
 * Iterates two linked lists:
 *   1. The actor's own encounter clump (via
 * encounter_actor_iterator_new/encounter_actor_iterator_next on
 * actor->clump_handle at actor_record+0x34).  For each member:
 *        - skip if member handle == actor_handle (self)
 *        - skip if count >= max_count
 *        - skip if member has no object (member+0x18 == -1)
 *        - skip if member is already targeting something (member+0x158 != -1)
 *        Calls prop_get_active_by_unit_index(actor_handle,
 * member_object_handle) to get a staging handle, then ai_generate_line_of_fire_pill(@esi=entry,
 * @edi=object_handle, actor_handle_from_64ab0) to fill the slot.
 *
 *   2. A secondary prop/enemy list (via prop_iterator_new/prop_iterator_next).
 *      For each entry:
 *        - skip if entry+0x60 is nonzero (flag)
 *        - skip if entry+0x127 is nonzero (flag)
 *        - skip if weapon-slot type != 3 (entry+0x24)
 *        - skip if entry+0x110 != -1
 *        - verify via object_get_and_verify_type that object type bit 0 is set
 *        - skip if both are in the same encounter (same encounter handle)
 *        - skip if count >= max_count
 *        Calls ai_generate_line_of_fire_pill(@esi=entry, @edi=entry_object_handle,
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
 * Call-site verification table — call to ai_generate_line_of_fire_pill at 0x4149a:
 *   arg      | binary source         | C expr             | match?
 *   stack[0] | PUSH EAX (ret 64ab0) | actor_handle_64ab0 | YES
 *   @esi     | LEA ESI,[buf+cnt*40] | &buf[count]        | YES
 *   @edi     | MOV EDI,[EDI+0x18]   | member_object_hdl  | YES
 *
 * Call-site verification table — call to ai_generate_line_of_fire_pill at 0x41567:
 *   arg      | binary source         | C expr             | match?
 *   stack[0] | PUSH ECX ([EBP-0xc]) | local_10[0]        | YES
 *   @esi     | LEA ESI,[buf+cnt*40] | &buf[count]        | YES
 *   @edi     | MOV EDI,[EDI+0x18]   | prop_object_handle | YES */
int16_t ai_find_line_of_fire_friend_pills(int actor_handle, int16_t max_count,
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
    member = (char *)encounter_actor_iterator_next(iter_a);
    while (member) {
      if (iter_a[1] != actor_handle && count < max_count &&
          *(int *)(member + 0x18) != -1 && *(int *)(member + 0x158) == -1) {
        member_object_handle = *(int *)(member + 0x18);
        staging =
          prop_get_active_by_unit_index(actor_handle, member_object_handle);
        /* entry ptr = buf + count*0x28; EDI = member_object_handle */
        ai_generate_line_of_fire_pill(&buf[count], member_object_handle, staging);
        count++;
      }
      member = (char *)encounter_actor_iterator_next(iter_a);
    }
  }

  /* --- loop 2: prop / enemy list --- */
  prop_iterator_new(local_10, actor_handle);
  prop = (char *)prop_iterator_next(local_10);
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
            ai_generate_line_of_fire_pill(&buf[count], prop_obj_handle, local_10[0]);
            count++;
          }
        }
      }
    }
    prop = (char *)prop_iterator_next(local_10);
  }

  return count;
}

/* ai_firing_pos_entry_t: see types.h for layout. */

/* ai_test_line_of_fire: test whether the actor can fire at a target through any
 * candidate firing position, and return the best candidate handle.
 *
 * Builds up to 0x20 candidate firing-position entries via ai_find_line_of_fire_friend_pills
 * (collecting nearby cover points / target-prop positions), then for each
 * entry:
 *   - skips entries whose handle_b matches excluded_handle (param_2)
 *   - if entry.is_sphere: calls fast_vector_intersects_sphere (line-sphere
 * intersection test)
 *   - otherwise:          calls vector_intersects_pill3d (segment-segment
 * proximity test) On the first passing test, marks that entry as occupied,
 * stores its handle_a as the result datum, clears the success flag, and breaks.
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
 *   the ai_find_line_of_fire_friend_pills call+cleanup. EBX is callee-saved and used throughout.
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

  count = (int)(int16_t)ai_find_line_of_fire_friend_pills(actor_handle, 0x20, buf);

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
          hit =
            fast_vector_intersects_sphere(origin, offset, e->vec_a, e->radius);
        } else {
          hit = vector_intersects_pill3d(origin, offset, e->vec_a, e->vec_b,
                                         e->scalar_a);
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
    ai_debug_lineoffire_new(origin, offset);
    for (i = 0; i < count; i++) {
      ai_firing_pos_entry_t *e = &buf[i];
      ai_debug_lineoffire_addpill(e->vec_a, e->vec_b, e->radius, e->occupied);
    }
    ai_debug_lineoffire_success((char)success);
  }

  if (result_out) {
    *result_out = result_datum;
  }
  return (bool)success;
}

/* ai_clump (ai_clump): scan all active player-actor records to find
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
bool ai_clump(char param_1)
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
        if (((actor_t *)actor)->field_006) {
          unit_rec = (char *)((actor_t *)actor)->field_024;
        } else {
          unit_rec = (char *)((actor_t *)actor)->field_018;
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
        if (param_1 && ((actor_t *)actor)->control_fire_state == 0 &&
            ((actor_t *)actor)->state_action != 10) {
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
        if (((actor_t *)actor)->target_target_prop_index ==
            (int)iter.datum_handle) {
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
 * a player. Delegates entirely to ai_clump(0). Returns true if any
 * enemy has line-of-sight to a player, false otherwise.
 * Confirmed: PUSH 0 / CALL 0x42390 / ADD ESP,4 / RET; caller (0xa74f0)
 * checks the return value as a bool. */
bool ai_enemies_can_see_player(void)
{
  return ai_clump(0);
}

/* ai_enemies_attacking_player: unconditionally trigger a clump check with
 * flag=1. Thin wrapper around ai_clump (ai_clump). Return value is
 * discarded by the caller. Confirmed: PUSH 1 / CALL 0x42390 / ADD ESP,4 / RET.
 */
void ai_enemies_attacking_player(void)
{
  ai_clump(1);
}

/* ai_handle_spatial_effect: ai_sound_spatial_effect_submit — submit a spatial
 * sound effect into the AI's ring buffer of recent spatial events. If AI
 * subsystem is inactive (g+1 == 0), returns immediately.
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
 * then write the entry and call actors_handle_spatial_effect to trigger the
 * actual AI reaction.
 *
 * Asserts: count > 0, 0 <= volume < 5, 0 <= effect_type < 3.
 * Confirmed: c:\halo\SOURCE\ai\ai.c line 0x80e/0x80f/0x810/0x847/0x871.
 */
void ai_handle_spatial_effect(int object_handle, float *position,
                              short effect_type, short volume, short count)
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
    actors_handle_spatial_effect(
      object_handle, *entry, (float *)((char *)entry + 0x4), volume, entry[1]);
  }
}
